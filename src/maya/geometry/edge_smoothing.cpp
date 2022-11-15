// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/geometry/edge_smoothing.hpp>

#if defined( WIN32 ) && defined( NDEBUG )
#include <intrin.h>
#pragma intrinsic( _BitScanForward )
#endif

#include <iostream>
#include <set>
#include <stack>
#include <vector>

#include <boost/dynamic_bitset.hpp>
#include <boost/random/lagged_fibonacci.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/unordered_set.hpp>

#include <maya/MDagPath.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItMeshEdge.h>

#include <frantic/diagnostics/assert_macros.hpp>
#include <frantic/diagnostics/profiling_manager.hpp>

namespace frantic {
namespace maya {
namespace geometry {

using boost::uint32_t;

// adjacency_list functions
void adjacency_list::soft_insert( uint32_t left, uint32_t right ) {
    // Ensure there is enough space in the vector
    const uint32_t larger = std::max( left, right ) + 1;
    const size_t size = m_softEntries.size();
    if( larger >= size ) {
        m_softEntries.resize( larger );
        m_hardEntries.resize( larger ); // We need to keep these two vectors the same size
        for( size_t i = size; i < larger; ++i ) {
            m_softEntries[i].reserve( 4 );
        }
    } else if( m_softEntries[larger - 1].capacity() < 4 ) {
        m_softEntries[larger - 1].reserve( 4 );
    }

    // Insert the elements
    m_softEntries[left].push_back( right );
    m_softEntries[right].push_back( left );
}

void adjacency_list::hard_insert( uint32_t left, uint32_t right ) {
    // Ensure there is enough space in the vector
    const uint32_t larger = std::max( left, right ) + 1;
    const size_t size = m_hardEntries.size();
    if( larger >= size ) {
        m_softEntries.resize( larger ); // We need to keep these two vectors the same size
        m_hardEntries.resize( larger );
        for( size_t i = size; i < larger; ++i ) {
            m_hardEntries[i].reserve( 4 );
        }
    } else if( m_hardEntries[larger - 1].capacity() < 4 ) {
        m_hardEntries[larger - 1].reserve( 4 );
    }

    // Insert the elements
    m_hardEntries[left].push_back( right );
    m_hardEntries[right].push_back( left );
}

// Check bounds (See below)
size_t adjacency_list::size() const {
    assert( m_softEntries.size() == m_hardEntries.size() && "Entries must be same size!" );
    return m_softEntries.size();
}

// group_list
// Used to keep track of which groups the nodes belong to,
// as well as the relationships between groups
// NOTE: Group 0 is the default group: everything belongs to it in the beginning and nothing belongs to it in the end
struct group_list {
    typedef boost::unordered_set<uint32_t> set; // TODO: std::set<u32> might be faster, need to check
    typedef boost::unordered_map<uint32_t, set> map;
    typedef map::const_iterator map_iterator;
    typedef std::vector<uint32_t>::const_iterator const_iterator;

    std::vector<uint32_t> groups;               // Each index represents a node, the element its group
    std::vector<std::vector<uint32_t>> members; // Each index represents a group, the elements its members
    map softNodes;
    map hardNodes;
    uint32_t _min;
    uint32_t _max; // The biggest and smallest group

    explicit group_list( int numFaces );

    inline bool has( uint32_t node ) const;
    inline uint32_t get( uint32_t node ) const;

    inline void join( uint32_t node, uint32_t group );

    // These keep track of the number and bounds (groups don't need to start at 1) of the groups
    inline uint32_t min() const;
    inline uint32_t max() const;

    inline uint32_t num_faces() const;
};

// Reserves memory for an unordered hashable data structure (unordered_map or unordered_set)
// TODO: unordered map and set have reserve() member functions as of Boost 1.50
template <typename T>
void unordered_reserve( T& unordered, size_t numElems ) {
    const size_t numBuckets = ( (size_t)( numElems / unordered.max_load_factor() ) ) + 1;
    unordered.rehash( numBuckets );
}

group_list::group_list( int numFaces )
    : groups( numFaces, 0 )
    , _min( 0 )
    , _max( 0 ) {
    unordered_reserve( softNodes, numFaces );
    unordered_reserve( hardNodes, numFaces );
    members.reserve( numFaces );
}

inline bool group_list::has( uint32_t node ) const {
    // Check if node is in the default group
    return groups[node] != 0;
}

inline uint32_t group_list::get( uint32_t node ) const { return groups[node]; }

// Assigns node a group, and includes all of its edges into the group
inline void group_list::join( uint32_t node, uint32_t group ) {
    // Assign the node the current group
    groups[node] = group;

    // Make sure there's enough space in members
    const size_t size = members.size();
    const size_t reqSize = group + 1;
    if( reqSize >= size ) {
        if( reqSize >= members.capacity() ) {
            members.reserve( group + group / 2 + 7 ); // Get at least 8 members into the array when reserving
        }
        members.resize( reqSize );
        for( size_t i = size; i < reqSize; ++i ) {
            members[i].reserve( 4 );
        }
    }
    members[group].push_back( node );

    // Update group maxs and mins
    if( _min == 0 && _max == 0 ) {
        _min = _max = group;
    } else if( _min > group ) {
        _min = group;
    } else if( _max < group ) {
        _max = group;
    }
}

inline uint32_t group_list::min() const { return _min; }
inline uint32_t group_list::max() const { return _max; }

inline uint32_t group_list::num_faces() const { return (uint32_t)groups.size(); }

// collapsed_cmp
// This is the functor used to compare groups while sorting
struct collapsed_cmp {
    const adjacency_list& edges;

    explicit collapsed_cmp( const adjacency_list& edges )
        : edges( edges ) {}

    inline bool operator()( const uint32_t& lhs, const uint32_t& rhs ) const;
};

// Compare by number of soft edges a group hases, and use hard edges as a tie-breaker
bool collapsed_cmp::operator()( const uint32_t& lhs, const uint32_t& rhs ) const {
    const size_t ssize1 = edges.soft_count( lhs );
    const size_t ssize2 = edges.soft_count( rhs );
    return ssize1 < ssize2 || ( ssize1 == ssize2 && edges.hard_count( lhs ) < edges.hard_count( rhs ) );
}

/**
 * Checks if a node can be safely added to a group
 *
 * @param begin
 * @param end
 * @param edges
 * @param group
 * @param groups
 * @return false if adding the node would break things
 */
inline bool has_set_intersection( const adjacency_list::const_iterator& begin,
                                  const adjacency_list::const_iterator& end, const group_list::set& edges,
                                  uint32_t group, group_list& groups ) {
    const size_t edgesSize = edges.size();
    if( edgesSize == 0 ) {
        // Every lookup will return edges.end(), so might as well bail out
        return true;
    }

    adjacency_list::const_iterator it;
    for( it = begin; it != end; ++it ) {
        const uint32_t current = *it;

        if( groups.groups[current] != group ) {

            if( edges.find( current ) != edges.end() ) {
                return false;
            }

            // Check the group of current to make sure there is no bad blood
            const uint32_t groupNumber = groups.get( current );
            if( groupNumber != 0 && groupNumber != group ) {
                // TODO: Use boost::disjoint_set?
                group_list::const_iterator git;

                const std::vector<uint32_t>& members = groups.members[groupNumber];
                for( git = members.begin(); git != members.end(); ++git ) {
                    if( edges.find( *git ) != edges.end() ) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

// Checks to see if the given graph_node is compatible with the given collapsed_node
inline bool node_check_merge( uint32_t node, uint32_t group, const adjacency_list& inputs, group_list& groups,
                              adjacency_list& outputs ) {
    // node cannot have soft nodes that have a hard edge with result
    if( !has_set_intersection( inputs.soft_begin( node ), inputs.soft_end( node ), groups.hardNodes[group], group,
                               groups ) )
        return false;
    // node cannot have hard nodes that have a soft edge with result
    if( !has_set_intersection( inputs.hard_begin( node ), inputs.hard_end( node ), groups.softNodes[group], group,
                               groups ) )
        return false;

    return true;
}

// Go through the graph to find all
inline void visit( uint32_t node, uint32_t group, const adjacency_list& inputs, group_list& groups,
                   adjacency_list& outputs ) {
    // TODO: Remember bad nodes?

    // Use this stack to do a DFS
    std::stack<uint32_t> to_visit;
    to_visit.push( node );

    group_list::set& softNodes = groups.softNodes[group];
    group_list::set& hardNodes = groups.hardNodes[group];

    while( to_visit.size() > 0 ) {
        uint32_t current = to_visit.top();
        to_visit.pop();

        // If current has no group, attempt to merge
        if( !groups.has( current ) && node_check_merge( current, group, inputs, groups, outputs ) ) {
            const size_t softSize = inputs.soft_count( current );
            const size_t hardSize = inputs.hard_count( current );

            // Merge groups
            adjacency_list::const_iterator it;
            for( it = inputs.soft_begin( current ); it != inputs.soft_end( current ); ++it ) {
                const uint32_t next = *it;
                const uint32_t groupNumber = groups.get( next );
                if( groupNumber == 0 ) {
                    // Add all of its soft nodes without a group to the stack to check
                    to_visit.push( next );
                } else if( groupNumber != group ) {
                    outputs.soft_insert( group, groupNumber );
                }
            }

            for( it = inputs.hard_begin( current ); it != inputs.hard_end( current ); ++it ) {
                const uint32_t next = *it;
                const uint32_t groupNumber = groups.get( next );
                if( groupNumber != 0 && groupNumber != group ) {
                    outputs.hard_insert( group, groupNumber );
                }
            }

            groups.join( current, group );

            // Get any new neighbours for result
            softNodes.insert( inputs.soft_begin( current ), inputs.soft_end( current ) );
            hardNodes.insert( inputs.hard_begin( current ), inputs.hard_end( current ) );

        } else if( groups.has( current ) && groups.get( current ) != group ) {
            // This is an unreachable case since a node is only pushed on the stack if it has no group,
            // and the only group it could have been given since it was pushed on the stack is 'group' (the variable)
            throw std::runtime_error( std::string( "Error in visit() - " __FILE__ "@" ) +
                                      boost::lexical_cast<std::string>( __LINE__ ) + ": Unreachable" );
        }
        // If 'current' has a group and it is equal to 'group', then it was done while waiting in the stack.
    }
}

// Collapse the nodes into a simplfied graph we can colour
inline adjacency_list collapse_graph( const adjacency_list& inputs, group_list& groups ) {
    uint32_t id = 1; // Starts at 1, 0 means no group
    adjacency_list outputs( groups.num_faces() );
    // Iterate through each input and collapse it
    unsigned i;
    for( i = 0; i < inputs.size(); ++i ) {
        // Only visit if it doesn't already have a group
        if( !groups.has( i ) ) {
            uint32_t groupNumber = id++;
            visit( i, groupNumber, inputs, groups, outputs );
        }
    }

    for( ; i < groups.num_faces(); ++i ) {
        uint32_t groupNumber = id++;
        groups.join( i, groupNumber );
    }

    // Make sure that the output has a vector to represent each node, even if it's empty
    uint32_t numGroups = groups.max() + 1;
    outputs.soft_ensure( numGroups );
    outputs.hard_ensure( numGroups );

    return outputs;
}

// Cross-platform wrapper for counting trailing zeros, undefined if mask is 0
inline uint32_t ctz( uint32_t mask ) {
    unsigned long index;
#if defined( WIN32 ) || defined( WIN64 )
    _BitScanForward( &index, mask );
#else
    index = __builtin_ctz( mask );
#endif

    return (uint32_t)index; // Index is less than 32, so the cast is fine
}

// This makes finding a flag fast
inline boost::uint32_t next_flag( boost::uint32_t mask ) {
    if( mask != ~0 ) {
        unsigned long index = ctz( ~mask );
        return (boost::uint32_t)1 << index;
    }
    return 0;
}

// The main function call
void color_graph( const adjacency_list& inputs, uint32_t numFaces, std::vector<uint32_t>& out ) {
    if( inputs.size() == 0 ) {
        // This is necessary to prevent the zero face case from crashing,
        // but it's helpful for all cases where there are no connections between nodes
        out.clear();
        out.resize( numFaces, 0 );
        return;
    }

    group_list groups( numFaces );
    adjacency_list collapsed = collapse_graph( inputs, groups );

    const size_t size = groups.max() - groups.min() + 1;
    std::vector<uint32_t> groupOrder;
    groupOrder.reserve( size );
    for( uint32_t j = groups.min(); j <= groups.max(); ++j ) {
        groupOrder.push_back( j );
    }

    // Sort based on number of soft nodes
    sort( groupOrder.rbegin(), groupOrder.rend(), collapsed_cmp( collapsed ) );

    // Assign bitflags
    std::vector<uint32_t>::iterator elem;
    std::vector<uint32_t> flags( size, 0 );
    std::vector<uint32_t> result( numFaces );
    std::pair<adjacency_list::const_iterator, adjacency_list::const_iterator> iters, innerIters;
    adjacency_list::const_iterator it, innerIt;
    const uint32_t offset = groups.min(); // RFC:(SBD): Offset, or just pad the vector?
    for( elem = groupOrder.begin(); elem != groupOrder.end() && collapsed.soft_count( *elem ) > 0; ++elem ) {
        const uint32_t current = *elem - offset;
        uint32_t bannedFlag = 0;

        // Go through the current nodes hard nodes to find all of the flags it can't have
        for( it = collapsed.hard_begin( *elem ); it != collapsed.hard_end( *elem ); ++it ) {
            bannedFlag |= flags[*it - offset];
        }

        // Go through its soft nodes to assign each pair a flag
        for( it = collapsed.soft_begin( *elem ); it != collapsed.soft_end( *elem ); ++it ) {
            const uint32_t visited = *it - offset;
            uint32_t otherBannedFlag = 0;

            // if false, they already have a flag in common, so we're done here
            if( ( flags[visited] & flags[current] ) == 0 ) {
                for( innerIt = collapsed.hard_begin( *it ); innerIt != collapsed.hard_end( *it ); ++innerIt ) {
                    otherBannedFlag |= flags[*innerIt - offset];
                }

                uint32_t currentFlag = next_flag( bannedFlag | otherBannedFlag );
                if( currentFlag != 0 ) {
                    flags[current] |= currentFlag;
                    flags[visited] |= currentFlag;
                } else {
                    throw std::runtime_error( "Current mesh's topology is too complicated to save smoothing groups" );
                }
            }
        }

        // Assign the flag to all our members
        const uint32_t flag = flags[current];
        group_list::const_iterator members;
        for( members = groups.members[*elem].begin(); members != groups.members[*elem].end(); ++members ) {
            result[*members] = flag;
        }
    }

    // This goes through all the nodes who only have hard nodes and sets their members smoothing groups
    for( ; elem != groupOrder.end(); ++elem ) {
        const uint32_t current = *elem - offset;
        const size_t size = groups.members[*elem].size();
        if( size <= 1 ) {
            flags[current] = 0; // Mesh is by itself, all of its edges are hard
        } else {
            uint32_t bannedFlag = 0;

            for( it = collapsed.hard_begin( *elem ); it != collapsed.hard_end( *elem ); ++it ) {
                bannedFlag |= flags[*it - offset];
            }

            uint32_t flag = next_flag( bannedFlag );
            if( flag != 0 ) {
                flags[current] = flag;
            } else {
                throw std::runtime_error( "Current mesh's topology is too complicated to save smoothing groups" );
            }
        }

        // Assign the flag to all our members
        const uint32_t flag = flags[current];
        group_list::const_iterator members;
        for( members = groups.members[*elem].begin(); members != groups.members[*elem].end(); ++members ) {
            result[*members] = flag;
        }
    }

    out.swap( result );
}

// Each index in the returned vector represents the same index in fnMesh's edges.
// The index stores a pair, consisting of a pair of intergers representing the vertices,
// and another vector, containing all of the faces connected to the edge (There should be 1 or 2 if the mesh is
// topologically correct)
std::vector<boost::tuple<int, int, std::vector<int>>> find_faces( const MFnMesh& fnMesh ) {

    MStatus stat;
    const int numVerts = fnMesh.numVertices();
    const int numFaces = fnMesh.numPolygons();
    std::vector<std::vector<int>> faceMap( numVerts );

    MIntArray mayaCounts;
    MIntArray mayaIndices;
    fnMesh.getVertices( mayaCounts, mayaIndices );

    for( int i = 0; i < numVerts; ++i ) {
        // There should usually be at most 6 faces connected to a vertex (hexagon made out of six triangles)
        faceMap[i].reserve( 6 );
    }

    unsigned int counter = 0;
    for( int i = 0; i < numFaces; ++i ) {
        for( int j = 0; j < mayaCounts[i]; ++j ) {
            int idx = mayaIndices[counter + j];
            faceMap[idx].push_back( i );
        }
        counter += mayaCounts[i];
    }

    std::vector<int> commonFaces;
    commonFaces.reserve( 3 ); // Expecting max two, three is a multiply connected edge
    const int numEdges = fnMesh.numEdges();

    std::vector<boost::tuple<int, int, std::vector<int>>> result;
    result.reserve( numEdges );

    for( int i = 0; i < numEdges; ++i ) {
        commonFaces.clear();

        int2 vertices;
        stat = fnMesh.getEdgeVertices( i, vertices );
        if( !stat )
            throw std::runtime_error( std::string( "Failed to get vertices: " ) + stat.errorString().asChar() );
        bool smooth = fnMesh.isEdgeSmooth( i, &stat );
        if( !stat )
            throw std::runtime_error( std::string( "Failed to get edge smoothness: " ) + stat.errorString().asChar() );

        int one = vertices[0];
        int two = vertices[1];

        const std::vector<int>& faces1 = faceMap[one];
        const std::vector<int>& faces2 = faceMap[two];

        std::vector<int>::const_iterator it1, it2, tmpIt;

        for( it1 = faces1.begin(); it1 != faces1.end(); ++it1 ) {
            int curr = *it1;

            for( it2 = faces2.begin(); it2 != faces2.end(); ++it2 ) {
                if( curr == *it2 ) {
                    commonFaces.push_back( curr );
                    break;
                }
            }
        }

        // tuple<int, int, vector<int>> would have been better than pair<pair<int, int>, vector<int>>, but it's not in
        // C++'03
        result.push_back( boost::make_tuple( one, two, commonFaces ) );
    }

    return result;
}

namespace testsuite {

// A test function, so there's an easy place to stick test code
bool test() {
    // Hard coded tests

    // For each test:
    // ARR_LENGTH is the number of elements in the array
    // NUM_NODES is the number of nodes in the graph (biggest node number + 1)
    // d(a, b, h) is an edge from a to b, where h==0 is a soft edge and h==1 is a hard edge. a must be less than b
    // TODO: These tests should check themselves, they currently just print out the result

    // In ascii drawings, - and | are horizontal and vertical soft edges, respectively
    // = and : are horizontal and vertical hard edges, respectively
    // / and \ are diagonal smooth edges, and ; is a diagonal hard edge

    // Vector of tests, contains the test name, number of nodes and a vector of connections
    std::vector<boost::tuple<std::string, int, std::vector<d>>> tests;

    // Empty case
    {
        const int ARR_LENGTH = 0, NUM_NODES = 0;
        tests.push_back( boost::make_tuple( "Empty case", NUM_NODES, std::vector<d>() ) );
    }

    // Single node
    // 0
    {
        const int ARR_LENGTH = 0, NUM_NODES = 1;
        tests.push_back( boost::make_tuple( "Single case", NUM_NODES, std::vector<d>() ) );
    }

    // Two unrelated nodes
    // 0   1
    {
        const int ARR_LENGTH = 0, NUM_NODES = 2;
        tests.push_back( boost::make_tuple( "Unconnected Nodes Case", NUM_NODES, std::vector<d>() ) );
    }

    // The simplest possible case (where the program doesn't just kick out right away)
    // 0 - 1
    {
        const int ARR_LENGTH = 1, NUM_NODES = 2;
        d arr[] = { d( 0, 1, 0 ) };
        tests.push_back( boost::make_tuple( "Easiest Soft Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // The simplest possible hard-edged case
    // 0 = 1
    {
        const int ARR_LENGTH = 1, NUM_NODES = 2;
        d arr[] = { d( 0, 1, 1 ) };
        tests.push_back( boost::make_tuple( "Easiest Hard Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Three nodes, one edge
    // 0 - 1   2
    {
        const int ARR_LENGTH = 1, NUM_NODES = 3;
        d arr[] = { d( 0, 1, 0 ) };
        tests.push_back( boost::make_tuple( "Drifting Node 1", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Same as last, different edge
    // 0 - 2   1
    {
        const int ARR_LENGTH = 1, NUM_NODES = 3;
        d arr[] = { d( 0, 2, 0 ) };
        tests.push_back( boost::make_tuple( "Drifting Node 2", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Cover all permutations
    // 0   1 - 2
    {
        const int ARR_LENGTH = 1, NUM_NODES = 3;
        d arr[] = { d( 1, 2, 0 ) };
        tests.push_back( boost::make_tuple( "Drifting Node 3", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Big Drifting case
    // 0  1 - 3   2   4
    {
        const int ARR_LENGTH = 1, NUM_NODES = 5;
        d arr[] = { d( 1, 3, 0 ) };
        tests.push_back(
            boost::make_tuple( "Large Drifting Node Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Easy case:
    // 0 - 1
    // |   :
    // 2 - 3
    {
        const int ARR_LENGTH = 4, NUM_NODES = 4;
        d arr[] = {
            d( 0, 1, 0 ),
            d( 0, 2, 0 ),
            d( 1, 3, 1 ),
            d( 2, 3, 0 ),
        };
        tests.push_back( boost::make_tuple( "Easy Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // The previous case with edge hardness reversed
    {
        const int ARR_LENGTH = 4, NUM_NODES = 4;
        d arr[] = {
            d( 0, 1, 1 ),
            d( 0, 2, 1 ),
            d( 1, 3, 0 ),
            d( 2, 3, 1 ),
        };
        tests.push_back( boost::make_tuple( "Inverse Easy Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Loop case
    // 0 - 1
    // |   |
    // 2 - 3
    {
        const int ARR_LENGTH = 4, NUM_NODES = 4;
        d arr[] = {
            d( 0, 1, 0 ),
            d( 0, 2, 0 ),
            d( 1, 3, 0 ),
            d( 2, 3, 0 ),
        };
        tests.push_back( boost::make_tuple( "Loop Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Hard Loop case
    // 0 = 1
    // :   :
    // 2 = 3
    {
        const int ARR_LENGTH = 4, NUM_NODES = 4;
        d arr[] = {
            d( 0, 1, 1 ),
            d( 0, 2, 1 ),
            d( 1, 3, 1 ),
            d( 2, 3, 1 ),
        };
        tests.push_back( boost::make_tuple( "Hard Loop Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // The Standard Test:
    // 0 -  1 =  2 -  3
    // :    |    |    :
    // 4 -  5 -  6 =  7
    // |    |    :    :
    // 8 =  9 - 10 - 11
    {
        const int ARR_LENGTH = 17, NUM_NODES = 12;
        d arr[] = { d( 0, 1, 0 ),  d( 0, 4, 1 ),  d( 1, 2, 1 ), d( 1, 5, 0 ),  d( 2, 3, 0 ),  d( 2, 6, 0 ),
                    d( 3, 7, 1 ),  d( 4, 5, 0 ),  d( 4, 8, 0 ), d( 5, 6, 0 ),  d( 5, 9, 0 ),  d( 6, 7, 1 ),
                    d( 6, 10, 1 ), d( 7, 11, 1 ), d( 8, 9, 1 ), d( 9, 10, 0 ), d( 10, 11, 0 ) };
        tests.push_back( boost::make_tuple( "Standard Test", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Reduced Standard test ( no bottom row )
    {
        const int ARR_LENGTH = 10, NUM_NODES = 8;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 4, 1 ), d( 1, 2, 1 ), d( 1, 5, 0 ), d( 2, 3, 0 ),
            d( 2, 6, 0 ), d( 3, 7, 1 ), d( 4, 5, 0 ), d( 5, 6, 0 ), d( 6, 7, 1 ),
        };
        tests.push_back(
            boost::make_tuple( "Reduced Standard Test", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Difficult case
    {
        const int ARR_LENGTH = 17, NUM_NODES = 8;
        d arr[] = {
            d( 0, 1, 1 ), d( 0, 2, 0 ), d( 0, 3, 0 ), d( 0, 4, 0 ), d( 0, 5, 0 ), d( 0, 6, 0 ),
            d( 1, 2, 0 ), d( 1, 3, 0 ), d( 1, 4, 0 ), d( 1, 5, 0 ), d( 1, 7, 0 ), d( 2, 3, 1 ),
            d( 3, 6, 1 ), d( 3, 7, 1 ), d( 4, 5, 1 ), d( 5, 6, 1 ), d( 5, 7, 1 ),
        };
        tests.push_back( boost::make_tuple( "Difficult Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Two unconnected meshes
    // 0 - 1 - 2 = 3
    //
    // 4 - 5 = 6 - 7
    {
        const int ARR_LENGTH = 6, NUM_NODES = 8;
        d arr[] = {
            d( 0, 1, 0 ), d( 1, 2, 0 ), d( 2, 3, 1 ), d( 4, 5, 0 ), d( 5, 6, 1 ), d( 6, 7, 0 ),
        };
        tests.push_back(
            boost::make_tuple( "Unconnected Meshes Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Ring case
    // 0 - 3 - 6 - 9
    // |   :   :   |
    // 1 = 4 = 7 = 10
    // |   :   :   |
    // 2 - 5 - 8 - 11
    {
        const int ARR_LENGTH = 17, NUM_NODES = 12;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 3, 0 ),  d( 1, 2, 0 ),  d( 1, 4, 1 ),  d( 2, 5, 0 ),   d( 3, 4, 1 ),
            d( 3, 6, 0 ), d( 4, 5, 1 ),  d( 4, 7, 0 ),  d( 5, 8, 0 ),  d( 6, 9, 0 ),   d( 6, 7, 1 ),
            d( 7, 8, 1 ), d( 7, 10, 1 ), d( 8, 11, 0 ), d( 9, 10, 0 ), d( 10, 11, 0 ),
        };
        tests.push_back( boost::make_tuple( "Ring Case 1", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Another (Softer) Ring test
    // 0 - 1 - 2
    // : /     |
    // 4       3
    // |     / |
    // 5 - 6 = 7
    {
        const int ARR_LENGTH = 10, NUM_NODES = 8;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 4, 1 ), d( 1, 2, 0 ), d( 1, 4, 0 ), d( 2, 3, 0 ),
            d( 3, 6, 0 ), d( 3, 7, 0 ), d( 4, 5, 0 ), d( 5, 6, 0 ), d( 6, 7, 1 ),
        };
        tests.push_back( boost::make_tuple( "Ring Case 2", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Yet Another Ring test
    // 0 - 1 - 2
    // : /   ; |
    // 4       3
    // | ;   / |
    // 5 - 6 = 7
    {
        const int ARR_LENGTH = 12, NUM_NODES = 8;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 4, 1 ), d( 1, 2, 0 ), d( 1, 3, 1 ), d( 1, 4, 0 ), d( 2, 3, 0 ),
            d( 3, 6, 0 ), d( 3, 7, 0 ), d( 4, 5, 0 ), d( 4, 6, 1 ), d( 5, 6, 0 ), d( 6, 7, 1 ),
        };
        tests.push_back( boost::make_tuple( "Ring Case 3", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Hole case
    {
        const int ARR_LENGTH = 15, NUM_NODES = 10;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 2, 0 ), d( 0, 7, 1 ), d( 1, 3, 0 ), d( 1, 9, 1 ),
            d( 2, 3, 1 ), d( 2, 4, 0 ), d( 3, 6, 0 ), d( 4, 5, 0 ), d( 4, 7, 0 ),
            d( 5, 6, 0 ), d( 5, 8, 0 ), d( 6, 9, 0 ), d( 7, 8, 1 ), d( 8, 9, 1 ),
        };
        tests.push_back( boost::make_tuple( "Hole Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Cube with one hard edge
    {
        const int ARR_LENGTH = 12, NUM_NODES = 6;
        d arr[] = {
            d( 0, 2, 0 ), d( 0, 3, 1 ), d( 0, 4, 0 ), d( 0, 5, 0 ), d( 1, 2, 0 ), d( 1, 3, 0 ),
            d( 1, 4, 0 ), d( 1, 5, 0 ), d( 2, 3, 0 ), d( 2, 5, 0 ), d( 3, 4, 0 ), d( 4, 5, 0 ),
        };
        tests.push_back( boost::make_tuple( "Cube Test", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Zigzag case
    //  0 -  1 -  2 -  3
    //  |    :    :    :
    //  4 -  5 -  6 -  7
    //  :    :    :    |
    //  8 -  9 - 10 - 11
    //  |    :    :    :
    // 12 - 13 - 14 - 15
    {
        const int ARR_LENGTH = 24, NUM_NODES = 16;
        d arr[] = {
            d( 0, 1, 0 ),   d( 0, 4, 0 ),   d( 1, 2, 0 ),   d( 1, 5, 1 ),   d( 2, 3, 0 ),   d( 2, 6, 1 ),
            d( 3, 7, 1 ),   d( 4, 5, 0 ),   d( 4, 8, 1 ),   d( 5, 6, 0 ),   d( 5, 9, 1 ),   d( 6, 7, 0 ),
            d( 6, 10, 1 ),  d( 7, 11, 0 ),  d( 8, 9, 0 ),   d( 8, 12, 0 ),  d( 9, 10, 0 ),  d( 9, 13, 1 ),
            d( 10, 11, 0 ), d( 10, 14, 1 ), d( 11, 15, 1 ), d( 12, 13, 0 ), d( 13, 14, 0 ), d( 14, 15, 0 ),
        };
        tests.push_back( boost::make_tuple( "Zigzag Case", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Szilassi polyhedron, the most complicated polyhedron I could find, with the worst possible edges
    {
        const int ARR_LENGTH = 21, NUM_NODES = 7;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 2, 0 ), d( 0, 3, 0 ), d( 0, 4, 0 ), d( 0, 5, 0 ), d( 0, 6, 0 ), d( 1, 2, 1 ),
            d( 1, 3, 1 ), d( 1, 4, 1 ), d( 1, 5, 1 ), d( 1, 6, 1 ), d( 2, 3, 1 ), d( 2, 4, 1 ), d( 2, 5, 1 ),
            d( 2, 6, 1 ), d( 3, 4, 1 ), d( 3, 5, 1 ), d( 3, 6, 1 ), d( 4, 5, 1 ), d( 4, 6, 1 ), d( 5, 6, 1 ),
        };
        tests.push_back(
            boost::make_tuple( "Szilassi Polyhedron", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Szilassi polyhedron, the most complicated polyhedron I could find, with the worst possible edges I could think of
    {
        const int ARR_LENGTH = 21, NUM_NODES = 7;
        d arr[] = {
            d( 0, 1, 1 ), d( 0, 2, 1 ), d( 0, 3, 0 ), d( 0, 4, 0 ), d( 0, 5, 0 ), d( 0, 6, 0 ), d( 1, 2, 0 ),
            d( 1, 3, 0 ), d( 1, 4, 0 ), d( 1, 5, 1 ), d( 1, 6, 0 ), d( 2, 3, 0 ), d( 2, 4, 0 ), d( 2, 5, 0 ),
            d( 2, 6, 1 ), d( 3, 4, 1 ), d( 3, 5, 0 ), d( 3, 6, 0 ), d( 4, 5, 0 ), d( 4, 6, 0 ), d( 5, 6, 0 ),
        };
        tests.push_back(
            boost::make_tuple( "Szilassi Polyhedron 2", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Szilassi polyhedron, the most complicated polyhedron I could find, with the worst possible edges I could think of
    {
        const int ARR_LENGTH = 21, NUM_NODES = 7;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 2, 1 ), d( 0, 3, 1 ), d( 0, 4, 1 ), d( 0, 5, 1 ), d( 0, 6, 1 ), d( 1, 2, 0 ),
            d( 1, 3, 1 ), d( 1, 4, 1 ), d( 1, 5, 1 ), d( 1, 6, 1 ), d( 2, 3, 0 ), d( 2, 4, 1 ), d( 2, 5, 1 ),
            d( 2, 6, 1 ), d( 3, 4, 0 ), d( 3, 5, 1 ), d( 3, 6, 1 ), d( 4, 5, 0 ), d( 4, 6, 1 ), d( 5, 6, 0 ),
        };
        tests.push_back(
            boost::make_tuple( "Szilassi Polyhedron 3", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Szilassi polyhedron, the most complicated polyhedron I could find, with the worst possible edges I could think of
    {
        const int ARR_LENGTH = 21, NUM_NODES = 7;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 2, 0 ), d( 0, 3, 0 ), d( 0, 4, 0 ), d( 0, 5, 0 ), d( 0, 6, 0 ), d( 1, 2, 1 ),
            d( 1, 3, 1 ), d( 1, 4, 0 ), d( 1, 5, 0 ), d( 1, 6, 0 ), d( 2, 3, 1 ), d( 2, 4, 0 ), d( 2, 5, 0 ),
            d( 2, 6, 0 ), d( 3, 4, 0 ), d( 3, 5, 0 ), d( 3, 6, 0 ), d( 4, 5, 1 ), d( 4, 6, 1 ), d( 5, 6, 1 ),
        };
        tests.push_back(
            boost::make_tuple( "Szilassi Polyhedron 4", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    // Szilassi polyhedron, the most complicated polyhedron I could find, with the worst possible edges I could think of
    {
        const int ARR_LENGTH = 21, NUM_NODES = 7;
        d arr[] = {
            d( 0, 1, 0 ), d( 0, 2, 0 ), d( 0, 3, 0 ), d( 0, 4, 1 ), d( 0, 5, 1 ), d( 0, 6, 1 ), d( 1, 2, 1 ),
            d( 1, 3, 1 ), d( 1, 4, 0 ), d( 1, 5, 0 ), d( 1, 6, 0 ), d( 2, 3, 1 ), d( 2, 4, 0 ), d( 2, 5, 0 ),
            d( 2, 6, 0 ), d( 3, 4, 0 ), d( 3, 5, 0 ), d( 3, 6, 0 ), d( 4, 5, 1 ), d( 4, 6, 1 ), d( 5, 6, 1 ),
        };
        tests.push_back(
            boost::make_tuple( "Szilassi Polyhedron 5", NUM_NODES, std::vector<d>( arr, arr + ARR_LENGTH ) ) );
    }

    std::vector<boost::tuple<std::string, int, std::vector<d>>>::const_iterator it;
    int counter = 0;
    int passed = 0;
    for( it = tests.begin(); it != tests.end(); ++it ) {
        bool success = true;

        const std::string& testName = boost::get<0>( *it );
        const int numNodes = boost::get<1>( *it );
        const std::vector<d>& data = boost::get<2>( *it );

        adjacency_list in;
        for( int i = 0; i < data.size(); ++i ) {
            const uint32_t a = data[i].a;
            const uint32_t b = data[i].b;

            if( data[i].h ) {
                in.hard_insert( a, b );
            } else {
                in.soft_insert( a, b );
            }
        }

        counter++;
        std::cout << std::endl << "Test " << counter << ": " << testName << std::endl;

        try {
            std::vector<uint32_t> outputs;
            color_graph( in, numNodes, outputs );

            for( int i = 0; i < data.size(); ++i ) {
                const d& curr = data[i];
                uint32_t r = outputs[curr.a] & outputs[curr.b];
                bool ok;
                if( curr.h ) {
                    ok = r == 0;
                } else {
                    ok = r != 0;
                }

                if( !ok ) {
                    std::cout << "Bad output: edge(" << curr.a << ", " << curr.b << ", " << curr.h << "), ";
                    std::cout << "got (" << outputs[curr.a] << " & " << outputs[curr.b] << ") == " << r << std::endl;
                    success = false;
                }
            }

            for( int i = 0; i < outputs.size(); ++i ) {
                std::cout << "At " << i << " (" << outputs[i] << "): ";
                for( int j = 1; outputs[i] > 0; ++j ) {
                    if( ( outputs[i] & 1 ) == 1 )
                        std::cout << j << ", ";
                    outputs[i] >>= 1;
                }
                std::cout << std::endl;
            }
        } catch( const std::runtime_error& e ) {
            std::cout << "Caught error: " << e.what() << std::endl;
            success = false;
        }

        if( success )
            passed++;
    }

    bool allTestsPassed = counter == passed;
    std::cout << "Done: ";
    if( allTestsPassed ) {
        std::cout << "All " << counter << " tests passed";
    } else {
        std::cout << "Failed " << ( counter - passed ) << " out of " << counter << " tests";
    }
    std::cout << std::endl;

    return allTestsPassed;
}

typedef MFloatPoint Point; // It's shorter

// QuadTree data structure used to genearte meshes
struct QuadTree {
    boost::shared_ptr<QuadTree> parent;
    boost::shared_ptr<QuadTree> ul;
    boost::shared_ptr<QuadTree> ur;
    boost::shared_ptr<QuadTree> bl;
    boost::shared_ptr<QuadTree> br;

    std::vector<int> data;

    float xmin;
    float xmax;
    float ymin;
    float ymax;

    explicit QuadTree( std::vector<int> inputs )
        : data( inputs ) {}

    inline static boost::shared_ptr<QuadTree>
    construct_tree( std::vector<Point*> inputs, std::vector<int> indices, float xmin, float xmax, float ymin,
                    float ymax, boost::shared_ptr<QuadTree> parent = boost::shared_ptr<QuadTree>() ) {
        if( indices.size() == 0 )
            return boost::shared_ptr<QuadTree>();

        boost::shared_ptr<QuadTree> tree = boost::shared_ptr<QuadTree>( new QuadTree( indices ) );
        tree->parent = parent;

        tree->xmin = xmin;
        tree->xmax = xmax;
        tree->ymin = ymin;
        tree->ymax = ymax;

        if( indices.size() > 1 ) {
            // Divide into subtrees
            std::vector<int>::const_iterator it;
            const float xmid = ( xmin + xmax ) / 2;
            const float ymid = ( ymin + ymax ) / 2;

            std::vector<int> vul, vur, vbl, vbr;

            for( it = indices.begin(); it != indices.end(); ++it ) {
                const int idx = *it;
                const float x = inputs[idx]->x;
                const float y = inputs[idx]->y;

                if( x < xmid && y >= ymid ) {
                    vul.push_back( idx );
                } else if( x >= xmid && y >= ymid ) {
                    vur.push_back( idx );
                } else if( x < xmid ) {
                    vbl.push_back( idx );
                } else {
                    vbr.push_back( idx );
                }
            }

            tree->ul = construct_tree( inputs, vul, xmin, xmid, ymid, ymax, tree );
            tree->ur = construct_tree( inputs, vur, xmid, xmax, ymid, ymax, tree );
            tree->bl = construct_tree( inputs, vbl, xmin, xmid, ymin, ymid, tree );
            tree->br = construct_tree( inputs, vbr, xmid, xmax, ymin, ymid, tree );
        } else {
            tree->ul = boost::shared_ptr<QuadTree>();
            tree->ur = boost::shared_ptr<QuadTree>();
            tree->bl = boost::shared_ptr<QuadTree>();
            tree->br = boost::shared_ptr<QuadTree>();
        }

        return tree;
    }

    // Pseudo-iterators
    inline QuadTree* first() {
        QuadTree* result = this;

        while( result->data.size() > 1 ) {
            if( result->ul )
                result = result->ul.get();
            else if( result->ur )
                result = result->ur.get();
            else if( result->bl )
                result = result->bl.get();
            else if( result->br )
                result = result->br.get();
            else
                std::runtime_error( "QuadTree::first() - Leaf node may not have more than one point" );
        }
        return result;
    }

    inline QuadTree* next() {
        assert( !this->ul && !this->ur && !this->bl && !this->br && "Can only call next() on a leaf node" );
        QuadTree* result = NULL;
        QuadTree* current = this;

        while( current ) {
            int pos;
            if( current->parent == NULL ) {
                current = NULL;
                break;
            }
            if( current == current->parent->ul.get() ) {
                pos = 0;
            } else if( current == current->parent->ur.get() ) {
                pos = 1;
            } else if( current == current->parent->bl.get() ) {
                pos = 2;
            } else if( current == current->parent->br.get() ) {
                pos = 3;
            } else {
                throw std::runtime_error( "QuadTree::next() - Got bad next pointer" );
            }

            if( pos <= 0 && current->parent->ur ) {
                current = current->parent->ur.get();
                break;
            } else if( pos <= 1 && current->parent->bl ) {
                current = current->parent->bl.get();
                break;
            } else if( pos <= 2 && current->parent->br ) {
                current = current->parent->br.get();
                break;
            } else {
                current = current->parent.get();
            }
        }

        if( current && current->data.size() > 1 ) {
            current = current->first();
        }

        return current;
    }
};

// Helper to find close points
inline void visit_sibling( const QuadTree* node, const QuadTree* last, const std::vector<Point*>& inputs, float x,
                           float y, std::vector<int>& resultVector ) {
    if( node && node != last && node->data.size() > 0 ) {
        for( int i = 0; i < node->data.size(); ++i ) {
            int idx = node->data[i];
            if( inputs[idx]->x > x )
                resultVector.push_back( node->data[i] );
        }
    }
}

// Get the nearest two points to the given tree (a leaf) and make a triangle
inline std::vector<int> gather_nearest( QuadTree* tree, const std::vector<Point*>& points ) {
    assert( tree || tree->data.size() == 0 && "Quadtree is empty" );
    assert( !tree->ul && !tree->ur && !tree->bl && !tree->br && "Can only call on a leaf node" );

    std::vector<int> results;

    QuadTree* current = tree->parent.get();
    if( current->parent )
        current = current->parent.get(); // Bump up another level, get some more of the surroundings
    QuadTree* last = tree;

    const float x = points[tree->data[0]]->x;
    const float y = points[tree->data[0]]->y;

    while( results.size() < 2 && current ) {
        visit_sibling( current->ul.get(), last, points, x, y, results );
        visit_sibling( current->ur.get(), last, points, x, y, results );
        visit_sibling( current->bl.get(), last, points, x, y, results );
        visit_sibling( current->br.get(), last, points, x, y, results );

        last = current;
        current = current->parent.get();
    }
    return results;
}

inline int GenerateTriangles( QuadTree* tree, const std::vector<Point*>& points, MIntArray& polygonConnects,
                              MStatus* status ) {
    int counter = 0;
    for( QuadTree* curr = tree->first(); curr; curr = curr->next() ) {
        assert( curr && "GenerateTriangles() - Got Null Tree" );

        std::vector<int> localPts = gather_nearest( curr, points );
        const Point& currPt = *points[curr->data[0]];

        if( localPts.size() >= 2 ) {
            int idx1 = localPts[0];
            int idx2 = localPts[1];
            Point& pt1 = *points[idx1];
            Point& pt2 = *points[idx2];
            float distance1 = currPt.distanceTo( pt1 );
            float distance2 = currPt.distanceTo( pt2 );

            if( distance2 < distance1 ) {
                std::swap( idx1, idx2 );
                std::swap( pt1, pt2 );
                std::swap( distance1, distance2 );
            }

            for( std::vector<int>::const_iterator it2 = localPts.begin() + 2; it2 != localPts.end(); ++it2 ) {
                int idx3 = *it2;
                Point& pt3 = *points[idx3];
                float distance3 = currPt.distanceTo( pt3 );
                if( distance3 < distance2 ) {
                    if( distance3 < distance1 ) {
                        idx2 = idx1;
                        distance2 = distance1;
                        pt2 = pt1;
                        idx1 = idx3;
                        distance1 = distance3;
                        pt1 = pt3;
                    } else {
                        idx2 = idx3;
                        distance2 = distance3;
                        pt2 = pt3;
                    }
                }
            }

            polygonConnects.append( curr->data[0] );
            polygonConnects.append( idx1 );
            polygonConnects.append( idx2 );

            ++counter;
        }
    }
    return counter;
}

std::vector<MObject> GeneratePlaneMesh( Point top_left, Point bottom_right, int x, int y, Pattern pattern ) {
    assert( x > 0 && y > 0 && "Invalid parameters" );

    const int numVertices = ( x + 1 ) * ( y + 1 );
    const int numPolygons = x * y;

    MFloatPointArray vertexArray;

    const float leftmost = top_left.x;
    const float topmost = top_left.y;
    const float xDistance = ( bottom_right.x - top_left.x ) / x;
    const float yDistance = ( top_left.y - bottom_right.y ) / y;

    for( int i = 0; i <= y; ++i ) {
        float currHeight = topmost - i * yDistance;
        float z = std::abs( sin( (float)M_PI * i / 2 ) ) * xDistance /
                  4.f; // This makes the edges a bit more visible. The sin() stuff is vintage, there's probably
                       // something simpler that does the same thing
        for( int j = 0; j <= x; ++j ) {
            vertexArray.append( leftmost + j * xDistance, currHeight, z );
        }
    }

    MIntArray polygonCounts( numPolygons, 4 );

    MIntArray polygonConnects;

    for( int i = 0; i < y; ++i ) {
        int xOffset = i * ( x + 1 );
        int rowOffset = ( i + 1 ) * ( x + 1 );
        for( int j = 0; j < x; ++j ) {
            polygonConnects.append( j + xOffset );
            polygonConnects.append( j + xOffset + 1 );
            polygonConnects.append( j + rowOffset + 1 );
            polygonConnects.append( j + rowOffset );
        }
    }

    const unsigned N = 4;
    std::vector<float> buf1( vertexArray.length() * N );
    vertexArray.get( ( float( * )[N] )( buf1.empty() ? 0 : &buf1[0] ) );

    std::vector<int> buf2( polygonCounts.length() );
    polygonCounts.get( buf2.empty() ? 0 : &buf2[0] );

    std::vector<int> buf3( polygonConnects.length(), 0 );
    polygonConnects.get( buf3.empty() ? 0 : &buf3[0] );

    MStatus stat;
    MDagPath dagPath;
    MFnMesh meshBuilder( dagPath );
    MObject newMesh = meshBuilder.create( numVertices, numPolygons, vertexArray, polygonCounts, polygonConnects,
                                          MObject::kNullObj, &stat );

    MIntArray mayaCounts;
    MIntArray mayaIndices;
    meshBuilder.getVertices( mayaCounts, mayaIndices );
    const int numEdges = meshBuilder.numEdges();
    std::vector<std::vector<int>> faceMap( numVertices );

    for( int i = 0; i < numVertices; ++i ) {
        faceMap[i].reserve( 6 );
    }

    unsigned int counter = 0;
    for( int i = 0; i < numPolygons; ++i ) {
        for( int j = 0; j < mayaCounts[i]; ++j ) {
            int idx = mayaIndices[counter + j];
            faceMap[idx].push_back( i );
        }
        counter += mayaCounts[i];
    }

    std::vector<int> common_faces;
    common_faces.reserve( 3 );
    for( int i = 0; i < numEdges; ++i ) {
        common_faces.clear();

        int2 vertices;
        meshBuilder.getEdgeVertices( i, vertices );

        int one = vertices[0];
        int two = vertices[1];

        const std::vector<int>& faces1 = faceMap[one];
        const std::vector<int>& faces2 = faceMap[two];

        std::vector<int>::const_iterator it1, it2, tmpIt;

        for( it1 = faces1.begin(); it1 != faces1.end(); ++it1 ) {
            int curr = *it1;

            for( it2 = faces2.begin(); it2 != faces2.end(); ++it2 ) {
                if( curr == *it2 ) {
                    common_faces.push_back( curr );
                    break;
                }
            }

            if( common_faces.size() >= 2 ) {
                bool smooth = pattern( common_faces[0], common_faces[1], x, y );
                meshBuilder.setEdgeSmoothing( i, smooth );
                break;
            }
        }
    }

    // Done smoothing stuff

    MString cmd( "sets -e -fe initialShadingGroup " );
    cmd += meshBuilder.name();
    MGlobal::executeCommand( cmd );

    std::vector<MObject> result;
    result.push_back( newMesh );
    return result;
}

// Geneartes a random mesh between top_left and bottom_right, with the give number of vertices, of specified hardness
// probability
std::vector<MObject> GenerateRandomTriangleMesh( Point top_left, Point bottom_right, int numVertices,
                                                 float fractionHard ) {
    typedef boost::lagged_fibonacci607 RNGType;
    typedef boost::uniform_real<float> Range;
    typedef boost::variate_generator<RNGType, Range> RNG;

    RNGType rng1( 12345 );
    RNGType rng2( 14427 );
    RNGType rng3( 3142592 );
    Range x_range( top_left.x, bottom_right.x );
    Range y_range( bottom_right.y, top_left.y );
    Range zero21( 0.0, 1.0 );

    RNG xGen( rng1, x_range );
    RNG yGen( rng2, y_range );
    RNG hGen( rng3, zero21 ); // handy generator :)

    MFloatPointArray points;
    points.setLength( numVertices );

    std::vector<Point*> pointVec( numVertices );
    std::vector<int> indices( numVertices );

    for( int i = 0; i < numVertices; i++ ) {
        points[i] = Point( xGen(), yGen(), hGen() );
        pointVec[i] = &points[i];
        indices[i] = i;
    }

    boost::shared_ptr<QuadTree> quad_tree =
        QuadTree::construct_tree( pointVec, indices, top_left.x, bottom_right.x, bottom_right.y, top_left.y );

    MIntArray polygonCounts, polygonConnects;
    MStatus stat;
    int numFaces = GenerateTriangles( quad_tree.get(), pointVec, polygonConnects, &stat );

    MIntArray faceCounts( numFaces, 3 );

    MDagPath dagPath;
    MFnMesh meshBuilder( dagPath );
    MObject newMesh =
        meshBuilder.create( numVertices, numFaces, points, faceCounts, polygonConnects, MObject::kNullObj, &stat );

    for( int i = 0; i < meshBuilder.numEdges(); ++i ) {
        meshBuilder.setEdgeSmoothing( i, hGen() > fractionHard );
    }

    meshBuilder.updateSurface();

    MString cmd( "sets -e -fe initialShadingGroup " );
    cmd += meshBuilder.name();
    MGlobal::executeCommand( cmd );

    std::vector<MObject> results;
    results.push_back( newMesh );
    return results;
}

// A fancy hash (finalizer) I found on the internet (See http://en.wikipedia.org/wiki/MurmurHash)
inline boost::uint32_t genNum( boost::uint32_t a ) {
    a = a ^ ( a >> 16 );
    a = a * 0x85ebca6b;
    a = a ^ ( a >> 13 );
    a = a * 0xc2b2ae35;
    a = a ^ ( a >> 16 );
    return a;
}

// Loads an .obj file, sets a bunch of the edges to random hardness and returns it all
std::vector<MObject> LoadObjRandomHardness( std::string filename, float fractionHard ) {
    MStatus stat;

    const std::string cmdStr =
        std::string( "file -f -options \"mo=1\" -typ \"OBJ\" -o " ) + frantic::strings::get_quoted_string( filename );

    MString cmd( cmdStr.c_str() );
    MGlobal::executeCommand( cmd );

    std::vector<MObject> results;
    MItDependencyNodes it( MFn::kMesh );

    // It's a bit broken. Investigating...
    while( !it.isDone() ) {
        MObject obj = it.thisNode( &stat );
        if( obj.isNull() ) {
            break;
        }

        MFnMesh mesh( obj, &stat );

        MItMeshEdge meshIt( obj, &stat );

        const boost::uint32_t percentage = ( boost::uint32_t )( fractionHard * 100 );
        int i = 0;
        while( !meshIt.isDone( &stat ) ) {
            FRANTIC_ASSERT_THROW( stat, std::string( "Error loading .obj file: " ) + stat.errorString().asChar() );
            boost::uint32_t num = genNum( i ) % 100;
            stat = meshIt.setSmoothing( num < percentage );
            FRANTIC_ASSERT_THROW( stat, std::string( "Error loading .obj file: " ) + stat.errorString().asChar() );
            i++;
            stat = meshIt.next();
            FRANTIC_ASSERT_THROW( stat, std::string( "Error loading .obj file: " ) + stat.errorString().asChar() );
        }
        stat = meshIt.updateSurface();
        FRANTIC_ASSERT_THROW( stat, std::string( "Error loading .obj file: " ) + stat.errorString().asChar() );

        MObject meshTransform = mesh.parent( 0, &stat );

        results.push_back( meshTransform );
        it.next();
    }

    return results;
}

std::vector<MObject> SimpleCubeMesh( int edge ) {
    const int squareSides = 4;
    const int numPolygons = 6;
    const int numVertices = 8;
    MFloatPointArray vertexArray;
    MIntArray polygonCounts( numPolygons, squareSides );
    MIntArray polygonConnects;

    vertexArray.append( -1, -1, -1 );
    vertexArray.append( 1, -1, -1 );
    vertexArray.append( 1, -1, 1 );
    vertexArray.append( -1, -1, 1 );
    vertexArray.append( -1, 1, -1 );
    vertexArray.append( -1, 1, 1 );
    vertexArray.append( 1, 1, 1 );
    vertexArray.append( 1, 1, -1 );

    int arr[] = { 0, 1, 2, 3, 4, 5, 6, 7, 3, 2, 6, 5, 0, 3, 5, 4, 0, 4, 7, 1, 1, 7, 6, 2 };
    for( int i = 0; i < numPolygons * squareSides; ++i ) {
        polygonConnects.append( arr[i] );
    }

    MStatus stat;
    MDagPath dagPath;
    MFnMesh meshBuilder( dagPath );
    MObject newMesh = meshBuilder.create( numVertices, numPolygons, vertexArray, polygonCounts, polygonConnects,
                                          MObject::kNullObj, &stat );

    FRANTIC_ASSERT_THROW( stat, std::string( "Failed to created plane mesh: " ) + stat.errorString().asChar() );

    meshBuilder.setEdgeSmoothing( edge, true );
    meshBuilder.setEdgeSmoothing( edge + 1, false );
    meshBuilder.updateSurface();

    MString cmd( "sets -e -fe initialShadingGroup " );
    cmd += meshBuilder.name();
    MGlobal::executeCommand( cmd );

    std::vector<MObject> results;
    results.push_back( newMesh );
    return results;
}

} // namespace testsuite

} // namespace geometry
} // namespace maya
} // namespace frantic
