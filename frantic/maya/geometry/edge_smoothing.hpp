// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <boost/unordered_map.hpp>
//#include <boost/unordered_set.hpp>
#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

#include <maya/MFloatPoint.h>
#include <maya/MFnMesh.h>

namespace frantic {
namespace maya {
namespace geometry {
// Represents the relation ships between the nodes/groups in the mesh.
class adjacency_list {
  public:
    typedef std::vector<boost::uint32_t>::const_iterator const_iterator;

  private:
    // One vector for the soft connections, and one for the hard connections
    // Each index in the vector corresponds to a node number, and contains a vector of all the nodes it has a connection
    // too It is used both for node relationships as well as group relationships
    std::vector<std::vector<boost::uint32_t>> m_softEntries;
    std::vector<std::vector<boost::uint32_t>> m_hardEntries;

  public:
    adjacency_list() {}

    explicit adjacency_list( boost::uint32_t capacity ) {
        m_softEntries.reserve( capacity );
        m_hardEntries.reserve( capacity );
    }

    void soft_insert( boost::uint32_t left, boost::uint32_t right );
    void hard_insert( boost::uint32_t left, boost::uint32_t right );

    size_t size() const;

    // IMPORTANT: None of these do bounds checking; it's up to you to ensure that there are enough entries
    inline size_t soft_count( boost::uint32_t entry ) const { return m_softEntries[entry].size(); }
    inline size_t hard_count( boost::uint32_t entry ) const { return m_hardEntries[entry].size(); }

    // These allow you to ensure the vectors have the right size
    inline void soft_ensure( size_t size ) { m_softEntries.resize( std::max( size, m_softEntries.size() ) ); }
    inline void hard_ensure( size_t size ) { m_hardEntries.resize( std::max( size, m_hardEntries.size() ) ); }

    // These also do not do bound checking
    inline const_iterator soft_begin( boost::uint32_t entry ) const { return m_softEntries[entry].begin(); }
    inline const_iterator soft_end( boost::uint32_t entry ) const { return m_softEntries[entry].end(); }
    inline const_iterator hard_begin( boost::uint32_t entry ) const { return m_hardEntries[entry].begin(); }
    inline const_iterator hard_end( boost::uint32_t entry ) const { return m_hardEntries[entry].end(); }
};

void color_graph( const adjacency_list& inputs, boost::uint32_t numFaces, std::vector<boost::uint32_t>& result );

std::vector<boost::tuple<int, int, std::vector<int>>> find_faces( const MFnMesh& fnMesh );

namespace testsuite {
// These are for building meshes
struct d {
    unsigned a;
    unsigned b;
    bool h;

    d()
        : a( 0 )
        , b( 0 )
        , h( 0 ) {}
    d( unsigned a, unsigned b, bool h )
        : a( a )
        , b( b )
        , h( h ) {}
};

bool test();

typedef MFloatPoint Point;

// Returns true if edge should be smooth
// The first face number, the second face number, the maximum value of x and the maximum value of y
typedef boost::function<bool( int, int, int, int )> Pattern;

// Generates a random mesh between top_left and bottom_right, with the give number of vertices, of specified hardness
// probability
std::vector<MObject> GenerateRandomTriangleMesh( Point top_left, Point bottom_right, int numVertices,
                                                 float fractionHard );

// Loads an .obj file, sets a bunch of the edges to random hardness and returns it all
std::vector<MObject> LoadObjRandomHardness( std::string filename, float fractionHard );

// Generates a big plane. Like a big rectangle, not an aircraft
std::vector<MObject> GeneratePlaneMesh( Point top_left, Point bottom_right, int x, int y, Pattern pattern );

// Returns a cube, with the specified edge being hard
std::vector<MObject> SimpleCubeMesh( int edge );

} // namespace testsuite
} // namespace geometry
} // namespace maya
} // namespace frantic
