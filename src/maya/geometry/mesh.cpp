// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/geometry/mesh.hpp>

#include <maya/MAnimControl.h>
#include <maya/MDagPath.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MUintArray.h>

#include <maya/MFnMeshData.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MItMeshPolygon.h>

#include <frantic/maya/attributes.hpp>
#include <frantic/maya/convert.hpp>
#include <frantic/maya/geometry/edge_smoothing.hpp>
#include <frantic/maya/graphics/maya_space.hpp>

#include <frantic/graphics/vector3.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/logging_level.hpp>

#include <frantic/geometry/mesh_interface_utils.hpp>
#include <frantic/geometry/polymesh3_builder.hpp>
#include <frantic/geometry/polymesh3_interface.hpp>

#include <frantic/diagnostics/profiling_manager.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/cstdint.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/unordered_set.hpp>

using namespace frantic::geometry;
using namespace frantic::graphics;
using namespace frantic::maya;
using namespace frantic::maya::graphics;

namespace {

bool is_valid_map_number( int mapNumber ) { return mapNumber > 0 && mapNumber < 100; }

bool get_map_number( const std::string& s, int* outMapNumber ) {
    using boost::algorithm::starts_with;

    if( !starts_with( s, "map" ) )
        return false;

    if( !outMapNumber )
        return false;

    bool gotMapNumber = false;
    try {
        const int mapNumber = boost::lexical_cast<int>( s.substr( 3 ) );
        if( is_valid_map_number( mapNumber ) ) {
            gotMapNumber = true;
            *outMapNumber = mapNumber;
        }
    } catch( boost::bad_lexical_cast& ) {
    }

    return gotMapNumber;
}

frantic::tstring get_map_channel_name( int mapNumber ) {
    if( !is_valid_map_number( mapNumber ) )
        throw std::runtime_error( "get_map_channel_name Error: map number " +
                                  boost::lexical_cast<std::string>( mapNumber ) + " is out of range" );

    if( mapNumber == 1 )
        return _T("TextureCoord");
    else
        return _T("Mapping") + boost::lexical_cast<frantic::tstring>( mapNumber );
}

boost::optional<boost::int32_t> try_get_constant_smoothing_group( const MFnMesh& fnMesh ) {
    MStatus stat;

    bool hasSoftEdge = false;
    bool hasHardEdge = false;
    for( int i = 0, ie = fnMesh.numEdges(); i < ie; ++i ) {
        const bool smooth = fnMesh.isEdgeSmooth( i, &stat );
        if( !stat )
            throw std::runtime_error( std::string( "Failed to get edge smoothness: " ) + stat.errorString().asChar() );

        if( smooth ) {
            hasSoftEdge = true;
        } else {
            hasHardEdge = true;
        }

        if( hasSoftEdge && hasHardEdge )
            break;
    }

    if( hasHardEdge != hasSoftEdge )
        return boost::optional<boost::int32_t>( hasHardEdge ? 0 : 1 );

    return boost::optional<boost::int32_t>();
}

boost::array<int, 2> make_array( int a, int b ) {
    boost::array<int, 2> result = { a, b };
    return result;
}

// In edgeToFaces, "no face" is indicated by the value -1.
void get_edge_to_faces( const MFnMesh& fnMesh, std::vector<boost::array<int, 2>>& edgeToFaces ) {
    MStatus stat;

    MIntArray mayaCounts;
    MIntArray mayaIndices;
    fnMesh.getVertices( mayaCounts, mayaIndices );

    const int numEdges = fnMesh.numEdges();
    const int numVerts = fnMesh.numVertices();
    const int numFaces = fnMesh.numPolygons();
    const int numFaceVerts = fnMesh.numFaceVertices();

    edgeToFaces.resize( numEdges, make_array( -1, -1 ) );

    std::vector<std::vector<int>> faceMap( numVerts );

    for( int i = 0; i < numVerts; ++i ) {
        faceMap[i].reserve( 6 );
    }

    unsigned int counter = 0;
    for( int i = 0; i < numFaces; ++i ) {
        for( int j = 0, je = mayaCounts[i]; j < je; ++j ) {
            int idx = mayaIndices[counter + j];
            faceMap[idx].push_back( i );
        }
        counter += mayaCounts[i];
    }

    std::vector<int> commonFaces;
    commonFaces.reserve( 3 );
    for( int i = 0; i < numEdges; ++i ) {
        commonFaces.clear();

        int2 vertices;
        stat = fnMesh.getEdgeVertices( i, vertices );
        if( !stat )
            throw std::runtime_error( std::string( "Failed to get vertices: " ) + stat.errorString().asChar() );

        const std::vector<int>& faces1 = faceMap[vertices[0]];
        const std::vector<int>& faces2 = faceMap[vertices[1]];

        std::set_intersection( faces1.begin(), faces1.end(), faces2.begin(), faces2.end(),
                               std::back_inserter( commonFaces ) );

        if( commonFaces.size() == 1 ) {
            edgeToFaces[i] = make_array( commonFaces[0], -1 );
        } else if( commonFaces.size() >= 2 ) {
            edgeToFaces[i] = make_array( commonFaces[0], commonFaces[1] );
        }
    }
}

void copy_mesh_geometry( MFloatPointArray& vertexArray, const frantic::geometry::trimesh3& mesh ) {
    const unsigned int vertexCount = static_cast<unsigned int>( mesh.vertex_count() );

    vertexArray.setLength( vertexCount );

    for( unsigned int i = 0; i < vertexCount; ++i ) {
        const frantic::graphics::vector3f v( mesh.get_vertex( i ) );
        vertexArray.set( i, v.x, v.y, v.z );
    }
}

void apply_velocity_offset( MFloatPointArray& vertexArray, const frantic::geometry::trimesh3& mesh, float timeOffset ) {
    const unsigned int vertexCount = static_cast<unsigned int>( mesh.vertex_count() );

    frantic::geometry::const_trimesh3_vertex_channel_cvt_accessor<frantic::graphics::vector3f> acc(
        mesh.get_vertex_channel_cvt_accessor<frantic::graphics::vector3f>( _T("Velocity") ) );
    for( unsigned int i = 0; i < vertexCount; ++i ) {
        const MFloatPoint& p = vertexArray[i];
        const frantic::graphics::vector3f dp = timeOffset * acc.get( i );
        vertexArray.set( i, p.x + dp.x, p.y + dp.y, p.z + dp.z );
    }
}

void copy_mesh_topology( MIntArray& polygonCounts, MIntArray& polygonConnects,
                         const frantic::geometry::trimesh3& mesh ) {
    const unsigned int faceCount = static_cast<unsigned int>( mesh.face_count() );

    polygonCounts.setLength( faceCount );
    polygonConnects.setLength( 3 * faceCount );

    for( unsigned int i = 0; i < faceCount; ++i ) {
        polygonCounts[i] = 3;
    }

    for( unsigned int faceIndex = 0; faceIndex < faceCount; ++faceIndex ) {
        const frantic::graphics::vector3 f( mesh.get_face( faceIndex ) );
        for( unsigned int corner = 0; corner < 3; ++corner ) {
            polygonConnects[3 * faceIndex + corner] = f[corner];
        }
    }
}

class no_color_transform {
  public:
    const frantic::graphics::color3f& operator()( const frantic::graphics::color3f& v ) const { return v; }
};

class scale_color_transform {
    float m_scale;

  public:
    scale_color_transform( double scale = 1.0 )
        : m_scale( static_cast<float>( scale ) ) {}

    frantic::graphics::color3f operator()( const frantic::graphics::color3f& v ) const { return m_scale * v; }
};

template <class ColorTransform>
void copy_mesh_color( MFnMesh& fnMesh, const frantic::tstring destColorSetName, const frantic::geometry::trimesh3& mesh,
                      const frantic::tstring srcChannelName, ColorTransform colorTransform ) {
    MStatus stat;

    if( !mesh.has_vertex_channel( srcChannelName ) ) {
        throw std::runtime_error( "copy_mesh_color Error: the source mesh does not have the required \'" +
                                  frantic::strings::to_string( srcChannelName ) + "\' channel." );
    }

    MString colorSet( destColorSetName.c_str() );

    stat = fnMesh.createColorSetDataMesh( colorSet );

    if( !stat ) {
        throw std::runtime_error( "copy_mesh_color Error: unable to create color set: " +
                                  std::string( stat.errorString().asChar() ) );
    }

    frantic::geometry::const_trimesh3_vertex_channel_cvt_accessor<frantic::graphics::color3f> acc(
        mesh.get_vertex_channel_cvt_accessor<frantic::graphics::color3f>( srcChannelName ) );

    const unsigned int colorCount = static_cast<unsigned int>( acc.size() );

    MColorArray colors;
    colors.setLength( colorCount );
    for( unsigned int i = 0; i < colorCount; ++i ) {
        colors[i] = frantic::maya::to_maya_t( colorTransform( acc.get( i ) ) );
    }
    stat = fnMesh.setColors( colors, &colorSet, MFnMesh::kRGB );
    if( !stat ) {
        throw std::runtime_error( "copy_mesh_color Error: unable to set colors: " +
                                  std::string( stat.errorString().asChar() ) );
    }

    const unsigned int faceCount = static_cast<unsigned int>( mesh.face_count() );

    MIntArray colorIds;
    colorIds.setLength( 3 * faceCount );
    for( unsigned int faceIndex = 0; faceIndex < faceCount; ++faceIndex ) {
        for( unsigned int corner = 0; corner < 3; ++corner ) {
            const unsigned int i = 3 * faceIndex + corner;
            colorIds[i] = acc.face( faceIndex )[corner];
        }
    }
    stat = fnMesh.assignColors( colorIds, &colorSet );
    if( !stat ) {
        throw std::runtime_error( "copy_mesh_color Error: unable to assign colors: " +
                                  std::string( stat.errorString().asChar() ) );
    }
}

void copy_mesh_color( MFnMesh& fnMesh, const frantic::tstring destColorSetName, const frantic::geometry::trimesh3& mesh,
                      const frantic::tstring srcChannelName ) {
    copy_mesh_color( fnMesh, destColorSetName, mesh, srcChannelName, no_color_transform() );
}

void copy_mesh_normals( MFnMesh& fnMesh, const frantic::geometry::trimesh3& mesh,
                        const frantic::tstring srcChannelName = _T("Normal") ) {
    MStatus stat;

    if( !mesh.has_vertex_channel( srcChannelName ) ) {
        throw std::runtime_error( "copy_mesh_normals Error: the source mesh does not have the required \'" +
                                  frantic::strings::to_string( srcChannelName ) + "\' channel." );
    }

    frantic::geometry::const_trimesh3_vertex_channel_cvt_accessor<frantic::graphics::vector3f> acc(
        mesh.get_vertex_channel_cvt_accessor<frantic::graphics::vector3f>( _T("Normal") ) );

    MVectorArray normalArray;

    normalArray.setLength( static_cast<unsigned int>( acc.size() ) );

    for( unsigned int i = 0; i < acc.size(); ++i ) {
        const frantic::graphics::vector3f normal = acc.get( i );
        normalArray[i] = frantic::maya::to_maya_t( normal );
    }

    const unsigned int faceCount = static_cast<unsigned int>( mesh.face_count() );

    MIntArray vertexCounts;
    MIntArray vertexIndices;
    fnMesh.getVertices( vertexCounts, vertexIndices );

    MVectorArray expandedNormalArray;
    MIntArray expandedFaceArray;

    unsigned int vertexIndicesLength = vertexIndices.length();

    expandedNormalArray.setLength( vertexIndicesLength );
    expandedFaceArray.setLength( vertexIndicesLength );

    for( unsigned int faceIndex = 0; faceIndex < faceCount; ++faceIndex ) {
        for( unsigned int corner = 0; corner < 3; ++corner ) {
            expandedNormalArray[faceIndex * 3 + corner] = normalArray[acc.face( faceIndex )[corner]];
            expandedFaceArray[faceIndex * 3 + corner] = faceIndex;
        }
    }

    stat = fnMesh.setFaceVertexNormals( expandedNormalArray, expandedFaceArray, vertexIndices );

    if( !stat ) {
        throw std::runtime_error( "copy_mesh_normals Error: unable to assign normals: " +
                                  std::string( stat.errorString().asChar() ) );
    }
}

void copy_mesh_texture_coord( MFnMesh& fnMesh, const frantic::geometry::trimesh3& mesh,
                              const frantic::tstring srcChannelName = _T("TextureCoord") ) {
    MStatus stat;

    if( !mesh.has_vertex_channel( srcChannelName ) ) {
        throw std::runtime_error( "copy_mesh_texture_coord Error: the source mesh does not have the required \'" +
                                  frantic::strings::to_string( srcChannelName ) + "\' channel." );
    }

    frantic::geometry::const_trimesh3_vertex_channel_cvt_accessor<frantic::graphics::vector3f> acc(
        mesh.get_vertex_channel_cvt_accessor<frantic::graphics::vector3f>( _T("TextureCoord") ) );

    MFloatArray uArray;
    MFloatArray vArray;
    uArray.setLength( static_cast<unsigned int>( acc.size() ) );
    vArray.setLength( static_cast<unsigned int>( acc.size() ) );

    for( unsigned int i = 0; i < acc.size(); ++i ) {
        const frantic::graphics::vector3f uvw = acc.get( i );
        uArray[i] = uvw[0];
        vArray[i] = uvw[1];
    }

    // stat = fnMesh.clearUVs();
    // if( !stat ){
    //	throw std::runtime_error( "copy_mesh_texture_coord Error: unable to clear UVs: " + std::string(
    //stat.errorString().asChar() ) );
    // }
    stat = fnMesh.setUVs( uArray, vArray );
    if( !stat ) {
        throw std::runtime_error( "copy_mesh_texture_coord Error: unable to set UVs: " +
                                  std::string( stat.errorString().asChar() ) );
    }

    const unsigned int faceCount = static_cast<unsigned int>( mesh.face_count() );

    MIntArray uvCounts;
    MIntArray uvIds;
    uvCounts.setLength( faceCount );
    uvIds.setLength( 3 * faceCount );

    for( unsigned int faceIndex = 0; faceIndex < faceCount; ++faceIndex ) {
        uvCounts[faceIndex] = 3;
        for( unsigned int corner = 0; corner < 3; ++corner ) {
            const unsigned int i = 3 * faceIndex + corner;
            uvIds[i] = acc.face( faceIndex )[corner];
        }
    }

    stat = fnMesh.assignUVs( uvCounts, uvIds );
    if( !stat ) {
        throw std::runtime_error( "copy_mesh_texture_coord Error: unable to assign UVs: " +
                                  std::string( stat.errorString().asChar() ) );
    }
}

double get_fps() { return MTime( 1.0, MTime::kSeconds ).as( MTime::uiUnit() ); }

int sum( const MIntArray& array ) {
    int result = 0;
    for( unsigned int i = 0, ie = array.length(); i != ie; ++i ) {
        result += array[i];
    }
    return result;
}

unsigned int get_instance_number( const MDagPath& dagPath ) {
    MStatus stat;

    unsigned int instanceNumber = dagPath.instanceNumber( &stat );
    if( !stat ) {
        throw std::runtime_error( "get_instance_number Error: unable to get instance number" );
    }

    return instanceNumber;
}

MString get_current_uv_set_name( const MDagPath& dagPath ) {
    MStatus stat;

    MFnMesh fnMesh( dagPath, &stat );
    if( !stat ) {
        throw std::runtime_error( "get_current_uv_set_name Error: unable to get mesh from dag path" );
    }

    MString currentUVSetName = fnMesh.currentUVSetName( &stat, get_instance_number( dagPath ) );
    if( !stat ) {
        throw std::runtime_error( "get_current_uv_set_name Error: unable to get current UV set name" );
    }

    return currentUVSetName;
}

MString get_current_color_set_name( const MDagPath& dagPath ) {
    MStatus stat;

    MFnMesh fnMesh( dagPath, &stat );
    if( !stat ) {
        throw std::runtime_error( "get_current_color_set_name Error: unable to get mesh from dag path" );
    }

    MString currentColorSetName = fnMesh.currentColorSetName( get_instance_number( dagPath ), &stat );
    if( !stat ) {
        throw std::runtime_error( "get_current_color_set_name Error: unable to get current color set name" );
    }

    return currentColorSetName;
}

bool contains( const MStringArray& stringArray, const MString& s ) {
    for( unsigned int i = 0, ie = stringArray.length(); i < ie; ++i ) {
        if( stringArray[i] == s ) {
            return true;
        }
    }

    return false;
}

bool has_uv_set( const MFnMesh& fnMesh, const MString& uvSetName ) {
    MStatus stat;
    MStringArray setNames;

    stat = fnMesh.getUVSetNames( setNames );
    if( !stat ) {
        throw std::runtime_error( "has_uv_set Error: unable to get uv set names" );
    }

    return contains( setNames, uvSetName );
}

bool has_color_set( const MFnMesh& fnMesh, const MString& colorSetName ) {
    MStatus stat;
    MStringArray setNames;

    stat = fnMesh.getColorSetNames( setNames );
    if( !stat ) {
        throw std::runtime_error( "has_color_set Error: unable to get color set names" );
    }

    return contains( setNames, colorSetName );
}

void copy_map( frantic::geometry::polymesh3_ptr destMesh, const frantic::tstring destChannelName,
               const MFnMesh& srcMesh, const MString& srcChannelName ) {
    MStatus stat;

    const std::string uvName( srcChannelName.asChar() );

    MIntArray uvCounts, uvIndices;
    MFloatArray uData, vData;

    stat = srcMesh.getUVs( uData, vData, &srcChannelName );
    if( !stat )
        throw std::runtime_error( "copy_map Error: Could not get the UVs from the UV set: \"" + uvName + "\"" );
    if( uData.length() != vData.length() )
        throw std::runtime_error( "copy_map Error: Mismatch between size of u array and v array in UV set: \"" +
                                  uvName + "\"" );
    // don't add the channel if it doesn't contain any data
    if( uData.length() == 0 )
        return;

    stat = srcMesh.getAssignedUVs( uvCounts, uvIndices, &srcChannelName );
    if( !stat )
        throw std::runtime_error( "copy_map Error: Could not get the UV indices from the UV set: \"" + uvName + "\"" );
    // don't add the channel if no faces have assigned UVs
    if( sum( uvCounts ) == 0 )
        return;

    destMesh->add_empty_vertex_channel( destChannelName, frantic::channels::data_type_float32, 3,
                                        (std::size_t)uData.length() );
    frantic::geometry::polymesh3_vertex_accessor<vector3f> chAcc =
        destMesh->get_vertex_accessor<vector3f>( destChannelName );

    if( chAcc.face_count() != uvCounts.length() )
        throw std::runtime_error( "copy_map Error: The number of UV polygons for UV set: \"" + uvName +
                                  "\" differs from geometry polygons" );

    for( std::size_t i = 0, iEnd = chAcc.vertex_count(); i < iEnd; ++i )
        chAcc.get_vertex( i ).set( uData[(int)i], vData[(int)i], 0.f );

    int counter = 0;
    for( std::size_t i = 0, iEnd = chAcc.face_count(); i < iEnd; counter += uvCounts[(unsigned)i], ++i )
        std::copy( &uvIndices[counter], &uvIndices[counter + uvCounts[(unsigned)i]], chAcc.get_face( i ).first );
}

void copy_color( frantic::geometry::polymesh3_ptr destMesh, const frantic::tstring& destChannelName,
                 const MDagPath& srcPath, const MString& srcChannelName ) {
    MStatus stat;

    MFnMesh srcMesh( srcPath, &stat );
    if( !stat ) {
        throw std::runtime_error( "copy_color Error: unable to get mesh from dag path" );
    }

    if( !has_color_set( srcMesh, srcChannelName ) ) {
        throw std::runtime_error( "copy_color Error: mesh does not have color set: " +
                                  std::string( srcChannelName.asChar() ) );
    }

    MFnMesh::MColorRepresentation colorRepresentation = srcMesh.getColorRepresentation( srcChannelName, &stat );
    if( !stat ) {
        throw std::runtime_error( "color_color Error: unable to get color representation from color set: " +
                                  std::string( srcChannelName.asChar() ) );
    }

    if( colorRepresentation != MFnMesh::kRGB && colorRepresentation != MFnMesh::kRGBA ) {
        return;
    }

    MColorArray colorArray;
    MColor defaultColor( 0.0, 0.0, 0.0, 1.0 );
    stat = srcMesh.getColors( colorArray, &srcChannelName, &defaultColor );
    if( !stat ) {
        throw std::runtime_error( "copy_color Error: unable to get colors from color set: " +
                                  std::string( srcChannelName.asChar() ) );
    }
    // don't copy the channel if it doesn't contain any data
    if( colorArray.length() == 0 ) {
        return;
    }

    frantic::graphics::raw_byte_buffer colorBuffer;
    colorBuffer.resize( colorArray.length() * sizeof( frantic::graphics::color3f ) );
    frantic::graphics::color3f* colorBufferData = reinterpret_cast<frantic::graphics::color3f*>( colorBuffer.begin() );
    for( unsigned int i = 0, ie = colorArray.length(); i < ie; ++i ) {
        colorBufferData[static_cast<std::size_t>( i )] = frantic::maya::from_maya_t( colorArray[i] );
    }

    const std::size_t expectedFaceBufferSize = destMesh->face_vertex_count();

    bool hasAssignedVertex = false;
    std::vector<int> faceBuffer;
    faceBuffer.reserve( expectedFaceBufferSize );

    MItMeshPolygon itPoly( srcPath );
    MIntArray colorIndices;
    for( ; !itPoly.isDone(); itPoly.next() ) {
        stat = itPoly.getColorIndices( colorIndices, &srcChannelName );
        for( int i = 0, ie = colorIndices.length(); i < ie; ++i ) {
            int colorIndex = colorIndices[i];
            if( colorIndex < 0 ) {
                colorIndex = 0;
            } else {
                hasAssignedVertex = true;
            }
            faceBuffer.push_back( colorIndex );
        }
    }
    if( faceBuffer.size() != expectedFaceBufferSize ) {
        throw std::runtime_error(
            "copy_color Error: mismatch between size of destination mesh and color indices in color set: " +
            std::string( srcChannelName.asChar() ) );
    }
    // don't copy the channel if it doesn't have any assigned vertices
    if( !hasAssignedVertex ) {
        return;
    }

    destMesh->add_vertex_channel( destChannelName, frantic::channels::data_type_float32, 3, colorBuffer, &faceBuffer );
}

} // anonymous namespace

namespace frantic {
namespace maya {
namespace geometry {

struct vector3f_hash {
    inline std::size_t operator()( const vector3f& v ) const {
        size_t result = 14427;
        boost::hash_combine( result, v.x );
        boost::hash_combine( result, v.y );
        boost::hash_combine( result, v.z );
        return result;
    }
};

frantic::geometry::polymesh3_ptr polymesh_copy( const MDagPath& dagPath, bool worldSpace,
                                                const frantic::channels::channel_propagation_policy& cpp,
                                                bool colorFromCurrentColorSet, bool textureCoordFromCurrentUVSet ) {
    MStatus stat;

    MFnMesh fnMesh( dagPath, &stat );
    if( !stat ) {
        throw std::runtime_error( "polymesh_copy Error: unable to get mesh from dag path" );
    }

    const int numVerts = fnMesh.numVertices();
    const int numFaces = fnMesh.numPolygons();
    const int numFaceVerts = fnMesh.numFaceVertices();

    polymesh3_builder polyBuild;

    const MSpace::Space space = worldSpace ? MSpace::kWorld : MSpace::kObject;

    // copy vertices
    MFloatPointArray mayaVerts;
    fnMesh.getPoints( mayaVerts, space );

    for( int i = 0; i < numVerts; ++i )
        polyBuild.add_vertex( frantic::maya::from_maya_t( mayaVerts[i] ) );

    MIntArray mayaCounts;
    MIntArray mayaIndices;
    fnMesh.getVertices( mayaCounts, mayaIndices );

    // copy faces
    unsigned int counter = 0;
    for( int i = 0; i < numFaces; ++i ) {
        polyBuild.add_polygon( &mayaIndices[counter], mayaCounts[i] );
        counter += mayaCounts[i];
    }

    frantic::geometry::polymesh3_ptr result = polyBuild.finalize();

    // copy Color channel
    const frantic::tstring colorChannel( _T("Color") );
    if( cpp.is_channel_included( colorChannel ) ) {
        const MString colorSetName = colorFromCurrentColorSet ? get_current_color_set_name( dagPath ) : "color";
        if( colorSetName.length() > 0 && has_color_set( dagPath, colorSetName ) ) {
            copy_color( result, colorChannel, dagPath, colorSetName );
        }
    }

    // copy map channels
    if( textureCoordFromCurrentUVSet ) {
        const frantic::tstring textureCoordChannel( _T("TextureCoord") );
        if( cpp.is_channel_included( textureCoordChannel ) ) {
            const MString currentUVSetName = get_current_uv_set_name( dagPath );
            if( currentUVSetName.length() > 0 && has_uv_set( dagPath, currentUVSetName ) ) {
                copy_map( result, textureCoordChannel, fnMesh, currentUVSetName );
            }
        }
    }

    MStringArray uvNames;
    stat = fnMesh.getUVSetNames( uvNames );
    if( !stat )
        throw std::runtime_error( "polymesh_copy Error: Could not get the UVSetNames from the mesh" );

    MIntArray uvCounts, uvIndices;
    MFloatArray uData, vData;
    for( unsigned uvNameIndex = 0; uvNameIndex < uvNames.length(); ++uvNameIndex ) {
        const std::string uvName( uvNames[uvNameIndex].asChar() );

        int mapNumber = 0;
        if( !get_map_number( uvName, &mapNumber ) )
            continue;

        if( mapNumber == 1 && textureCoordFromCurrentUVSet )
            continue;

        const frantic::tstring channelName = get_map_channel_name( mapNumber );

        if( result->has_vertex_channel( channelName ) )
            continue;

        if( cpp.is_channel_included( channelName ) )
            copy_map( result, channelName, fnMesh, uvNames[uvNameIndex] );
    }

    // copy vertex normals
    frantic::tstring normalsChannel = _T("Normal");
    if( cpp.is_channel_included( normalsChannel ) ) {
        // SBD: Constant flag for now, make adjustable later? Is there ever any reason to not want to deduplicate
        // normals? I changed this to false for now, because it looks like we don't change normalIds to account for the
        // removed normals.
        const bool dedupNormals = false;
        frantic::geometry::polymesh3_vertex_accessor<vector3f> normalsAccessor;

        MFloatVectorArray normals;
        fnMesh.getNormals( normals, space );

        MIntArray normalCountsPerFace;
        MIntArray normalIds;
        fnMesh.getNormalIds( normalCountsPerFace, normalIds );

        if( dedupNormals ) {
            boost::unordered_set<vector3f, vector3f_hash> seenNormals;

            // Reserve space
            const size_t numNormals = normals.length();
            size_t numBuckets = ( (size_t)( numNormals / seenNormals.max_load_factor() ) ) + 1;
            seenNormals.rehash( numBuckets );

            for( unsigned i = 0; i < numNormals; ++i ) {
                seenNormals.insert( from_maya_space( frantic::maya::from_maya_t( normals[i] ) ) );
            }

            result->add_empty_vertex_channel( normalsChannel, frantic::channels::data_type_float32, 3,
                                              (std::size_t)seenNormals.size() );
            normalsAccessor = result->get_vertex_accessor<vector3f>( normalsChannel );

            boost::unordered_set<vector3f, vector3f_hash>::iterator it = seenNormals.begin();
            for( unsigned i = 0; i < seenNormals.size(); ++i, ++it ) {
                normalsAccessor.get_vertex( i ) = *it;
            }
        } else {
            result->add_empty_vertex_channel( normalsChannel, frantic::channels::data_type_float32, 3,
                                              (std::size_t)normals.length() );
            normalsAccessor = result->get_vertex_accessor<vector3f>( normalsChannel );

            for( unsigned int i = 0; i < normalsAccessor.vertex_count(); ++i ) {
                normalsAccessor.get_vertex( i ) = frantic::maya::from_maya_t( normals[i] );
            }
        }

        if( normalsAccessor.face_count() != normalCountsPerFace.length() ) {
            throw std::runtime_error(
                "polymesh_copy Error: The number of normal polygons differs from the geometry polygon count." );
        }

        counter = 0;
        for( unsigned int i = 0; i < normalsAccessor.face_count(); counter += normalCountsPerFace[i], ++i ) {
            std::copy( &normalIds[counter], &normalIds[counter + normalCountsPerFace[i]],
                       normalsAccessor.get_face( i ).first );
        }
    }

    // create MaterialID from connected shaders
    frantic::tstring materialIDChannel = _T("MaterialID");
    if( cpp.is_channel_included( materialIDChannel ) ) {
        MObjectArray shadersArray;
        MIntArray shaderIndexArray;
        stat = fnMesh.getConnectedShaders( 0, shadersArray, shaderIndexArray );

        // It seems that this will return false in the event that no shaders are connected, so we'll just ignore
        // material ids for now
        if( stat ) {
            if( shaderIndexArray.length() != result->face_count() )
                throw std::runtime_error( "polymesh_copy Error: Number of material mapping faces does not match the "
                                          "number of faces in the mesh." );

            result->add_empty_face_channel( materialIDChannel, frantic::channels::data_type_uint16, 1 );
            frantic::geometry::polymesh3_face_accessor<boost::uint16_t> materialIdAccess =
                result->get_face_accessor<boost::uint16_t>( materialIDChannel );

            for( size_t i = 0, iEnd = materialIdAccess.face_count(); i < iEnd; ++i )
                materialIdAccess.get_face( i ) = shaderIndexArray[(unsigned)i];
        }
    }

    return result;
}

void copy_edge_creases( const MDagPath& dagPath, const MFnMesh& srcMesh, polymesh3_ptr outMesh ) {
    MStatus stat;
    const frantic::tstring edgeCreaseChannelName = _T("EdgeSharpness");
    MUintArray edgeIds;
    MDoubleArray creaseData;
    std::map<std::pair<int, int>, int> vertsToBufferPos;

    if( srcMesh.numPolygons() != static_cast<int>( outMesh->face_count() ) ) {
        throw std::runtime_error(
            "copy_edge_creases Error: mismatch between number of faces in source mesh and destination mesh" );
    }

    stat = srcMesh.getCreaseEdges( edgeIds, creaseData );
    if( !stat &&
        stat.statusCode() != MStatus::kFailure ) { // MStatus::kFailure is returned if there are no creased edges
        throw std::runtime_error( "copy_edge_creases Error: unable to get edge creases from source mesh" );
    }

    // don't copy the channel if it doesn't contain any data
    if( creaseData.length() == 0 ) {
        return;
    }

    frantic::graphics::raw_byte_buffer edgeCreaseChannelBuffer;
    edgeCreaseChannelBuffer.resize( ( creaseData.length() + 1 ) * sizeof( float ) );
    float* edgeCreaseChannel = reinterpret_cast<float*>( edgeCreaseChannelBuffer.begin() );

    // add the crease data to the data buffer, and map between edges' vertex pairs and data buffer locations of their
    // crease magnitudes
    edgeCreaseChannel[creaseData.length()] = 0; // default (no creasing)
    for( unsigned int i = 0, ie = creaseData.length(); i < ie; ++i ) {
        edgeCreaseChannel[i] = creaseData[i];

        int vertexList[2];
        stat = srcMesh.getEdgeVertices( edgeIds[i], vertexList );
        if( !stat ) {
            throw std::runtime_error( "copy_edge_creases Error: unable to get edge vertices" );
        }
        if( vertexList[0] > vertexList[1] ) {
            std::swap( vertexList[0], vertexList[1] );
        }
        vertsToBufferPos[std::make_pair( vertexList[0], vertexList[1] )] = i;
    }

    const std::size_t expectedFaceBufferSize = outMesh->face_vertex_count();
    bool hasAssignedVertex = false;
    std::vector<int> faceBuffer;
    faceBuffer.reserve( expectedFaceBufferSize );

    MItMeshPolygon itPoly( dagPath );
    for( ; !itPoly.isDone(); itPoly.next() ) {
        MIntArray vertices;
        itPoly.getVertices( vertices );
        for( unsigned int i = 0, ie = vertices.length(); i < ie; ++i ) {

            int secondPos = ( i < vertices.length() - 1 ) ? secondPos = i + 1 : secondPos = 0;
            std::pair<int, int> edge = std::make_pair( vertices[i], vertices[secondPos] );
            if( edge.first > edge.second ) {
                std::swap( edge.first, edge.second );
            }

            int creasedEdgeIndex = creaseData.length();

            std::map<std::pair<int, int>, int>::iterator itVertsBuf = vertsToBufferPos.find( edge );
            if( itVertsBuf != vertsToBufferPos.end() && itVertsBuf->second <= creaseData.length() ) {
                // we have a creased edge for this vertex pair, so add its position in the data buffer to the face
                // buffer
                creasedEdgeIndex = itVertsBuf->second;
                if( creasedEdgeIndex < 0 ) {
                    creasedEdgeIndex = creaseData.length();
                } else {
                    hasAssignedVertex = true;
                }
            }
            faceBuffer.push_back( creasedEdgeIndex );
        }
    }
    if( faceBuffer.size() != expectedFaceBufferSize ) {
        throw std::runtime_error(
            "copy_edge_creases Error: mismatch between size of source mesh and destination mesh" );
    }

    if( !hasAssignedVertex ) {
        return;
    }

    outMesh->add_vertex_channel( edgeCreaseChannelName, frantic::channels::data_type_float32, 1,
                                 edgeCreaseChannelBuffer, &faceBuffer );
}

void copy_vertex_creases( const MDagPath& dagPath, const MFnMesh& srcMesh, polymesh3_ptr outMesh ) {
    MStatus stat;
    const frantic::tstring vertexCreaseChannelName = _T("VertexSharpness");
    MUintArray vertexIDs;
    MDoubleArray creaseData;
    std::map<int, float> vertToMagnitude;

    stat = srcMesh.getCreaseVertices( vertexIDs, creaseData );
    if( !stat &&
        stat.statusCode() != MStatus::kFailure ) { // MStatus::kFailure is returned if there are no creased vertices
        throw std::runtime_error( "copy_vertex_creases Error: unable to get vertex creases from source mesh" );
    }

    // don't copy the channel if it doesn't contain any data
    if( creaseData.length() == 0 ) {
        return;
    }

    unsigned int numVertices = outMesh->vertex_count();

    if( srcMesh.numVertices() != numVertices ) {
        throw std::runtime_error(
            "\ncopy_vertex_creases Error: Source and destination meshes must have the same number of vertices" );
    }

    frantic::graphics::raw_byte_buffer vertexCreaseChannelBuffer;
    vertexCreaseChannelBuffer.resize( numVertices * sizeof( float ) );
    float* vertexCreaseChannel = reinterpret_cast<float*>( vertexCreaseChannelBuffer.begin() );
    memset( vertexCreaseChannel, 0, numVertices * sizeof( float ) );

    for( int i = 0; i < creaseData.length(); ++i ) {
        vertexCreaseChannel[vertexIDs[i]] = creaseData[i];
    }

    outMesh->add_vertex_channel( vertexCreaseChannelName, frantic::channels::data_type_float32, 1,
                                 vertexCreaseChannelBuffer );
}

void create_smoothing_groups( const MFnMesh& fnMesh, polymesh3_ptr outMesh ) {
    std::vector<boost::uint32_t> prevEncoding;

    create_smoothing_groups( fnMesh, prevEncoding, outMesh );
}

// Could this vertex cause erroneous smoothing between incident faces?
bool may_have_crosstalk( const std::vector<int>& vertexDiscontinuities, int vertexIndex ) {
    return vertexDiscontinuities[vertexIndex] > 3;
}

bool has_hard_edge( const adjacency_list& inputs, int a, int b ) {
    for( adjacency_list::const_iterator i = inputs.hard_begin( a ), ie = inputs.hard_end( a ); i != ie; ++i ) {
        if( *i == b ) {
            return true;
        }
    }
    return false;
}

struct crosstalk_vertex_info {
    typedef std::set<int> connected_faces_t;
    typedef std::vector<int> connected_edges_t;

    connected_faces_t faces;
    connected_edges_t edges;
};

// Without this function, we can get the same smoothing group on two faces
// which share the same vertex, and which are separated by a hard edge (or
// by a boundary), but which do not share the same hard edge.  This can
// happen if there are more than three hard edges incident on a vertex.
//
// For example (the lines are hard edges, and the number is the
// resulting smoothing group):
//
// Before:
// +---+---+
// | 1 | 2 |
// +---+---+
// | 2 | 1 |
// +---+---+
//
// And after this function adds a hard edge between diagonally-opposed faces:
// +---+---+
// | 1 | 2 |
// +---+---+
// | 3 | 4 |
// +---+---+
//
void add_cross_vertex_hard_edges( const MFnMesh& fnMesh, const std::vector<boost::array<int, 2>>& edgeToFaces,
                                  adjacency_list& inputs ) {
    MStatus stat;

    const int numEdges = fnMesh.numEdges();
    const int numVerts = fnMesh.numVertices();
    const int numFaces = fnMesh.numPolygons();
    const int numFaceVerts = fnMesh.numFaceVertices();

    std::vector<int> vertexDiscontinuities( numVerts );
    for( int edgeIndex = 0; edgeIndex < numEdges; ++edgeIndex ) {
        bool smooth = fnMesh.isEdgeSmooth( edgeIndex, &stat );
        if( !stat ) {
            throw std::runtime_error( std::string( "Failed to get edge smoothness: " ) + stat.errorString().asChar() );
        }

        boost::array<int, 2> faces = edgeToFaces[edgeIndex];

        const bool isBoundaryEdge = faces[0] >= 0 && faces[1] < 0;
        if( isBoundaryEdge || !smooth ) {
            int2 vertexList;
            stat = fnMesh.getEdgeVertices( edgeIndex, vertexList );
            for( int i = 0; i < 2; ++i ) {
                ++vertexDiscontinuities[vertexList[i]];
            }
        }
    }

    // I'm referring to the erroneous smoothing across faces that share
    // the same vertex as "crosstalk".
    std::vector<int> crosstalkToVertexIndex;
    for( int i = 0; i < numVerts; ++i ) {
        if( may_have_crosstalk( vertexDiscontinuities, i ) ) {
            crosstalkToVertexIndex.push_back( i );
        }
    }

    const std::size_t numCrosstalkVerts = crosstalkToVertexIndex.size();

    if( numCrosstalkVerts > 0 ) {
        typedef crosstalk_vertex_info::connected_faces_t connected_faces_t;
        typedef crosstalk_vertex_info::connected_edges_t connected_edges_t;

        std::vector<crosstalk_vertex_info> crosstalkVertexInfo( numCrosstalkVerts );
        { // scope for vertexToCrosstalkIndex
            std::vector<int> vertexToCrosstalkIndex( numVerts );
            for( int i = 0; i < numCrosstalkVerts; ++i ) {
                vertexToCrosstalkIndex[crosstalkToVertexIndex[i]] = i;
            }

            for( int edgeIndex = 0; edgeIndex < numEdges; ++edgeIndex ) {
                int2 vertexList;
                fnMesh.getEdgeVertices( edgeIndex, vertexList );
                BOOST_FOREACH( int vertexIndex, vertexList ) {
                    if( may_have_crosstalk( vertexDiscontinuities, vertexIndex ) ) {
                        boost::array<int, 2> faces = edgeToFaces[edgeIndex];
                        BOOST_FOREACH( int faceIndex, faces ) {
                            // If soft_count() is zero, then the face isn't going to get
                            // a smoothing group, so we can ignore it.
                            if( faceIndex >= 0 && inputs.soft_count( faceIndex ) > 0 ) {
                                const int crosstalkIndex = vertexToCrosstalkIndex[vertexIndex];
                                crosstalkVertexInfo[crosstalkIndex].faces.insert( faceIndex );
                                crosstalkVertexInfo[crosstalkIndex].edges.push_back( edgeIndex );
                            }
                        }
                    }
                }
            }
        }

        // Want a data structure with random access iterators, so that we can
        // map from a face index entry to its relative position in the
        // collection (which is equal to its index in the disjointSets array).
        typedef boost::container::flat_set<int> ordered_faces_t;
        ordered_faces_t connectedFaces;

        std::vector<int> ranks;
        std::vector<int> parents;
        std::vector<int> disjointSetFaces;

        for( int crosstalkIndex = 0; crosstalkIndex < numCrosstalkVerts; ++crosstalkIndex ) {
            const connected_faces_t& connectedFacesSet = crosstalkVertexInfo[crosstalkIndex].faces;
            const connected_edges_t& edgeList = crosstalkVertexInfo[crosstalkIndex].edges;

            connectedFaces.clear();
            connectedFaces.insert( connectedFacesSet.begin(), connectedFacesSet.end() );

            ranks.resize( connectedFaces.size() );
            parents.resize( connectedFaces.size() );

            // Disjoint set of the faces connected to this vertex
            boost::disjoint_sets<int*, int*> disjointSets( &ranks[0], &parents[0] );
            for( std::size_t i = 0, ie = connectedFaces.size(); i < ie; ++i ) {
                disjointSets.make_set( static_cast<int>( i ) );
            }

            // Union faces together if they're connected by a smooth edge
            BOOST_FOREACH( int edgeIndex, edgeList ) {
                bool isSmooth = fnMesh.isEdgeSmooth( edgeIndex, &stat );
                if( !stat ) {
                    throw std::runtime_error( "Unable to get edge smoothness" );
                }
                if( isSmooth ) {
                    boost::array<int, 2> edgeFaces = edgeToFaces[edgeIndex];
                    if( edgeFaces[0] >= 0 && edgeFaces[1] >= 0 ) {
                        boost::array<int, 2> disjointSetIndices;
                        for( int side = 0; side < 2; ++side ) {
                            const int faceIndex = edgeFaces[side];
                            ordered_faces_t::iterator i = connectedFaces.find( faceIndex );
                            if( i == connectedFaces.end() ) {
                                throw std::runtime_error( "Unable to find face in connected faces" );
                            }
                            disjointSetIndices[side] = static_cast<int>( i - connectedFaces.begin() );
                        }
                        disjointSets.union_set( disjointSetIndices[0], disjointSetIndices[1] );
                    }
                }
            }

            // Choose one face from each disjoint set
            disjointSetFaces.clear();
            for( int i = 0, ie = static_cast<int>( connectedFaces.size() ); i < ie; ++i ) {
                if( disjointSets.find_set( i ) == i ) {
                    const int faceIndex = *( connectedFaces.begin() + i );
                    disjointSetFaces.push_back( faceIndex );
                }
            }

            // Add a hard edge between each disjoint set
            for( int b = 0, be = static_cast<int>( disjointSetFaces.size() ); b < be; ++b ) {
                for( int a = 0; a < b; ++a ) {
                    int faceA = disjointSetFaces[a];
                    int faceB = disjointSetFaces[b];
                    // TODO: we probably want to avoid a linear search here,
                    // but normally the number of edges is small.
                    if( !has_hard_edge( inputs, faceA, faceB ) ) {
                        inputs.hard_insert( faceA, faceB );
                    }
                }
            }
        }
    }
}

void create_smoothing_groups( const MFnMesh& fnMesh, std::vector<boost::uint32_t>& encoding, polymesh3_ptr outMesh ) {
    MStatus stat;

    const frantic::tstring smoothingGroupChannelName = _T("SmoothingGroup");

    const int numEdges = fnMesh.numEdges();
    const int numVerts = fnMesh.numVertices();
    const int numFaces = fnMesh.numPolygons();
    const int numFaceVerts = fnMesh.numFaceVertices();

    if( numFaces != static_cast<int>( outMesh->face_count() ) ) {
        throw std::runtime_error(
            "create_smoothing_groups Error: mismatch between number of faces in fnMesh and outMesh" );
    }

    frantic::graphics::raw_byte_buffer smoothingGroupChannelBuffer;
    smoothingGroupChannelBuffer.resize( numFaces * sizeof( boost::int32_t ) );
    boost::int32_t* smoothingGroupChannel = reinterpret_cast<boost::int32_t*>( smoothingGroupChannelBuffer.begin() );

    boost::optional<boost::int32_t> constantSmoothingGroup = try_get_constant_smoothing_group( fnMesh );
    if( constantSmoothingGroup ) {
        boost::int32_t smoothingGroup( *constantSmoothingGroup );
        for( int i = 0; i < numFaces; ++i ) {
            smoothingGroupChannel[i] = smoothingGroup;
        }
    } else {
        std::vector<boost::array<int, 2>> edgeToFaces;
        get_edge_to_faces( fnMesh, edgeToFaces );

        // check if the old encoding works for this mesh
        bool identicalGroups = encoding.size() == numFaces;
        for( int i = 0; i < numEdges && identicalGroups; ++i ) {
            bool smooth = fnMesh.isEdgeSmooth( i, &stat );
            if( !stat )
                throw std::runtime_error( std::string( "Failed to get edge smoothness: " ) +
                                          stat.errorString().asChar() );

            boost::array<int, 2> faces = edgeToFaces[i];
            if( faces[0] >= 0 && faces[1] >= 0 ) {
                const bool prevEncodingSmooth = ( encoding[faces[0]] & encoding[faces[1]] ) != 0;
                if( prevEncodingSmooth != smooth ) {
                    identicalGroups = false;
                }
            }
        }

        // if the old encoding doesn't work, make a new one
        if( !identicalGroups ) {
            adjacency_list inputs( numFaces );
            for( int i = 0; i < numEdges; ++i ) {
                bool smooth = fnMesh.isEdgeSmooth( i, &stat );
                if( !stat )
                    throw std::runtime_error( std::string( "Failed to get edge smoothness: " ) +
                                              stat.errorString().asChar() );

                boost::array<int, 2> faces = edgeToFaces[i];
                if( faces[0] >= 0 && faces[1] >= 0 ) {
                    if( smooth ) {
                        inputs.soft_insert( faces[0], faces[1] );
                    } else {
                        inputs.hard_insert( faces[0], faces[1] );
                    }
                }
            }

            add_cross_vertex_hard_edges( fnMesh, edgeToFaces, inputs );

            color_graph( inputs, numFaces, encoding );
        }

        for( size_t i = 0, iEnd = static_cast<std::size_t>( numFaces ); i < iEnd; ++i ) {
            smoothingGroupChannel[i] = encoding[i];
        }
    }

    outMesh->add_face_channel( smoothingGroupChannelName, frantic::channels::data_type_int32, 1,
                               smoothingGroupChannelBuffer );
}

// used internally by copy_maya_mesh
void copy_maya_mesh_internal( MFnMesh& mayaMesh, trimesh3& outFranticMesh, bool generateNormals, bool generateUVCoords,
                              bool generateColors ) {
    outFranticMesh.clear();
    outFranticMesh.set_vertex_count( mayaMesh.numVertices() );
    MPointArray vertices;
    mayaMesh.getPoints( vertices );

    for( int i = 0; i < mayaMesh.numVertices(); ++i ) {
        outFranticMesh.get_vertex( i ) = frantic::maya::from_maya_t( vertices[i] );
    }

    MIntArray triangleCounts;
    MIntArray triangleVertices;
    mayaMesh.getTriangles( triangleCounts, triangleVertices );

    size_t triangleCountSum = 0;
    for( unsigned int i = 0; i < triangleCounts.length(); ++i ) {
        triangleCountSum += triangleCounts[i];
    }

    outFranticMesh.set_face_count( triangleCountSum );

    for( unsigned int i = 0; i < outFranticMesh.face_count(); ++i ) {
        outFranticMesh.get_face( i ) =
            vector3( triangleVertices[i * 3 + 0], triangleVertices[i * 3 + 1], triangleVertices[i * 3 + 2] );
    }

    if( generateColors ) {
        // generate colors from the polygons.
        MStatus stat;
        MString colorSetName = mayaMesh.currentColorSetName( &stat );
        if( !stat ) {
            throw std::runtime_error( "copy_maya_mesh_internal Error: unable to get mesh's color set name" );
        }

        if( colorSetName != _T("") ) {
            MFnMesh::MColorRepresentation colorRepresentation = mayaMesh.getColorRepresentation( colorSetName, &stat );
            if( !stat ) {
                throw std::runtime_error(
                    "copy_maya_mesh_internal Error: unable to get color representation from color set: " +
                    std::string( colorSetName.asChar() ) );
            }

            if( colorRepresentation != MFnMesh::kRGB && colorRepresentation != MFnMesh::kRGBA ) {
                throw std::runtime_error( "copy_maya_mesh_internal Error: color representation must be RGB or RGBA" );
            }

            MColorArray colorArray;
            MColor defaultColor( 0.0, 0.0, 0.0, 1.0 );
            stat = mayaMesh.getColors( colorArray, &colorSetName, &defaultColor );
            if( !stat ) {
                throw std::runtime_error( "copy_maya_mesh_internal Error: unable to get colors from color set: " +
                                          std::string( colorSetName.asChar() ) );
            }
            // don't copy the channel if it doesn't contain any data
            if( colorArray.length() != 0 ) {
                // add a color channel
                outFranticMesh.add_vertex_channel<vector3f>( _T("Color"), colorArray.length() + 1, true );
                trimesh3_vertex_channel_accessor<vector3f> colorAccessor =
                    outFranticMesh.get_vertex_channel_accessor<vector3f>( _T("Color") );

                // define the color data array
                colorAccessor[colorArray.length()] = vector3f( 0.0, 0.0, 0.0 );
                for( unsigned int i = 0; i < colorArray.length(); ++i ) {
                    colorAccessor[i] = vector3f( colorArray[i].r, colorArray[i].g, colorArray[i].b );
                }

                // now define the custom faces for the color array assigned above.
                size_t triangleIndex = 0;
                for( unsigned int polygonIndex = 0; polygonIndex < triangleCounts.length(); ++polygonIndex ) {
                    MIntArray polygonVertexIndices;
                    mayaMesh.getPolygonVertices( polygonIndex, polygonVertexIndices );

                    // create a map between the actual vertex indices, and their color index using their relative index
                    // on the polygon (this assumes that no polygon has any repeated vertex indices)
                    std::map<int, int> vertexIndexMap;
                    for( unsigned int i = 0; i < polygonVertexIndices.length(); ++i ) {
                        int colorIndex;
                        stat = mayaMesh.getColorIndex( polygonIndex, i, colorIndex, &colorSetName );
                        if( !stat ) {
                            throw std::runtime_error(
                                "copy_maya_mesh_internal Error: unable to get color index of vertex for color set: " +
                                std::string( colorSetName.asChar() ) );
                        }
                        if( colorIndex == -1 ) {
                            colorIndex = colorArray.length();
                        }

                        vertexIndexMap[polygonVertexIndices[i]] = colorIndex;
                    }

                    for( int i = 0; i < triangleCounts[polygonIndex]; ++i, ++triangleIndex ) {
                        vector3 colorIndices;
                        for( size_t j = 0; j < 3; ++j ) {
                            colorIndices[j] = vertexIndexMap[outFranticMesh.get_face( triangleIndex )[j]];
                        }
                        colorAccessor.face( triangleIndex ) = colorIndices;
                    }
                }
            }
        }
    } // generate colors

    if( generateNormals ) {
        // generate normals from the polygons.
        MFloatVectorArray normals;
        mayaMesh.getNormals( normals );
        frantic::graphics::vector3f currentNormal;

        // add a normals channel with custom faces
        outFranticMesh.add_vertex_channel<vector3f>( _T("Normal"), normals.length(), true );
        trimesh3_vertex_channel_accessor<vector3f> normalsAccessor =
            outFranticMesh.get_vertex_channel_accessor<vector3f>( _T("Normal") );

        // define the normals data array
        for( unsigned int i = 0; i < normals.length(); ++i ) {
            currentNormal = frantic::maya::from_maya_t( normals[i] );
            normalsAccessor[i] = frantic::maya::from_maya_t( normals[i] );
        }

        // now define the custom faces for the normals array assigned above.
        size_t triangleIndex = 0;
        for( unsigned int polygonIndex = 0; polygonIndex < triangleCounts.length(); ++polygonIndex ) {
            MIntArray polygonVertexIndices;
            mayaMesh.getPolygonVertices( polygonIndex, polygonVertexIndices );

            // create a map between the actual vertex indices, and their relative index on the polygon
            // (this assumes that no polygon has any repeated vertex indices)
            std::map<int, int> vertexIndexMap;

            for( unsigned int i = 0; i < polygonVertexIndices.length(); ++i ) {
                vertexIndexMap[polygonVertexIndices[i]] = i;
            }

            MIntArray faceVertexNormalIds;
            mayaMesh.getFaceNormalIds( polygonIndex, faceVertexNormalIds );

            for( int i = 0; i < triangleCounts[polygonIndex]; ++i, ++triangleIndex ) {
                vector3 normalIndices;
                for( size_t j = 0; j < 3; ++j ) {
                    normalIndices[j] = faceVertexNormalIds[vertexIndexMap[outFranticMesh.get_face( triangleIndex )[j]]];
                }
                normalsAccessor.face( triangleIndex ) = normalIndices;
            }
        }
    } // generate normals

    if( generateUVCoords ) {
        // generate UV coords from the polygons.
        MFloatArray uArray, vArray;
        mayaMesh.getUVs( uArray, vArray, NULL );

        // add a UV coords channel with custom faces
        outFranticMesh.add_vertex_channel<vector3f>( _T("TextureCoord"), uArray.length(), true );
        trimesh3_vertex_channel_accessor<vector3f> textureCoordAccessor =
            outFranticMesh.get_vertex_channel_accessor<vector3f>( _T("TextureCoord") );

        // define the UV coords data array
        for( unsigned int i = 0; i < uArray.length(); ++i ) {
            textureCoordAccessor[i] = vector3f( uArray[i], vArray[i], 0.0f );
        }

        // now define the custom faces for the UV coords array assigned above.
        size_t triangleIndex = 0;
        for( unsigned int polygonIndex = 0; polygonIndex < triangleCounts.length(); ++polygonIndex ) {
            MIntArray polygonVertexIndices;
            mayaMesh.getPolygonVertices( polygonIndex, polygonVertexIndices );

            // create a map between the actual vertex indices, and their relative index on the polygon
            // (this assumes that no polygon has any repeated vertex indices)
            std::map<int, int> vertexIndexMap;

            for( unsigned int i = 0; i < polygonVertexIndices.length(); ++i ) {
                vertexIndexMap[polygonVertexIndices[i]] = i;
            }

            for( int i = 0; i < triangleCounts[polygonIndex]; ++i, ++triangleIndex ) {
                vector3 uvCoordIndices;
                for( size_t j = 0; j < 3; ++j ) {
                    int uvId;
                    // get the face vertex uvId
                    mayaMesh.getPolygonUVid( polygonIndex, vertexIndexMap[outFranticMesh.get_face( triangleIndex )[j]],
                                             uvId, NULL );
                    uvCoordIndices[j] = uvId;
                }
                textureCoordAccessor.face( triangleIndex ) = uvCoordIndices;
            }
        }
    } // generate UV coords
}

// used internally by copy_maya_mesh
bool generate_vertex_velocities( const MFnMesh& fnNewMesh, frantic::geometry::trimesh3& outMesh,
                                 float timeStepInFrames ) {

    // first try to disqualify this mesh based on number of vertices.
    MStatus status;
    int oldNumVerts = (int)outMesh.vertex_count();
    int newNumVerts = fnNewMesh.numVertices( &status );
    if( newNumVerts != oldNumVerts ) {
        FF_LOG( debug ) << "Offset mesh had " << newNumVerts << " vertices, original mesh has " << oldNumVerts
                        << " vertices. Velocity not computed at this time offset.\n";
        return false;
    }

    /*
    //I did a bunch of tests on the face array... However, it looks like the face arrays are almost always different.
    //However, on tests, the vertex array seems to be always in the same order. This may be a problem in the future. I
    don't know just yet.
    //So, we are currently assuming that the vertex arrays are in the same order and that velocity can be generated
    simply by differencing the vertex-index pairs.

    //next try to disqualify this mesh based on an inconsistent face array.
    MIntArray triangleCounts;
    MIntArray triangleVertices;
    fnNewMesh.getTriangles( triangleCounts, triangleVertices );

    if( triangleVertices.length() != outMesh.face_count() * 3 ) {
            FF_LOG(debug) << "Offset mesh had a different number of faces than the original mesh. Velocity not computed
    at this time offset.\n"; return false;
    }
    for( unsigned int i = 0; i < outMesh.face_count(); ++i ) {
            const vector3& oldFace = outMesh.get_face( i );
            vector3 newFace( triangleVertices[i*3 + 0], triangleVertices[i*3 + 1], triangleVertices[i*3 + 2] );
            if( oldFace != newFace ) { //Make sure the face is the same for the original mesh, and our offset mesh.
                    FF_LOG(debug) << "Inconsistent triangle: Face #" << i << " was: " << oldFace.str() << ". It's now: "
    << newFace.str() << "\n";
                    //return false;
            }
    }
    */

    // Create the vertex channel.
    outMesh.add_vertex_channel<vector3f>( _T("Velocity") );
    trimesh3_vertex_channel_accessor<vector3f> velAcc = outMesh.get_vertex_channel_accessor<vector3f>( _T("Velocity") );

    MPointArray vertices;
    fnNewMesh.getPoints( vertices );

    float fps = (float)MTime( 1.0, MTime::kSeconds ).as( MTime::uiUnit() );
    float timeStep = fps / timeStepInFrames;

    // use differencing to compute vertex velocity.
    bool foundNonZeroVelocity = false;
    for( int i = 0; i < fnNewMesh.numVertices(); ++i ) {
        const MPoint& newVertex = vertices[i];
        velAcc[i] =
            ( vector3f( (float)newVertex.x, (float)newVertex.y, (float)newVertex.z ) - outMesh.get_vertex( i ) ) *
            timeStep;
        if( !foundNonZeroVelocity && velAcc[i] != vector3f( 0.0f ) )
            foundNonZeroVelocity = true;
    }

    // don't bother keeping the velocity channel if it's all zero.
    // there's no real reason to do this. just a little optimization.
    if( !foundNonZeroVelocity ) {
        outMesh.erase_vertex_channel( _T("Velocity" ) );
        FF_LOG( debug ) << "No vertex motion found for mesh at time offset " << timeStepInFrames << ".\n";
    } else {
        FF_LOG( debug ) << "Offset mesh found for velocity computation at time offset " << timeStepInFrames << ".\n";
    }

    return true;
}

void copy_maya_mesh( MPlug inPlug, frantic::geometry::trimesh3& outMesh, bool generateNormals, bool generateUVCoords,
                     bool generateVelocity, bool generateColors, bool useSmoothedMeshSubdivs ) {
    MStatus status;
    MObject baseMeshObj;
    inPlug.getValue( baseMeshObj );

    // Cast the object as a mesh (because it is a mesh).
    if( !baseMeshObj.hasFn( MFn::kMesh ) )
        throw std::runtime_error(
            "copy_maya_mesh error: The provided plug is not of kMesh type. Could not retrieve a mesh." );

    MFnMesh baseMesh( baseMeshObj, &status );

    FF_LOG( debug ) << "Calling copy_maya_mesh on \"" << baseMesh.name().asChar() << "\"\n";

    // determine if it's a smoothed mesh.
    // The "displaySmoothMesh" option can be 0,1,2 based on the check box smooth mesh preview and the radio buttons for
    // Display.
    bool isSmooth = useSmoothedMeshSubdivs && ( frantic::maya::get_int_attribute( baseMesh, "displaySmoothMesh" ) > 0 );

    // get the smoothed mesh options
    MFnMeshData parentMeshData;
    MObject parentObject;
    MMeshSmoothOptions smoothMeshOptions;
    if( isSmooth ) {
        parentObject = parentMeshData.create();
        baseMesh.getSmoothMeshDisplayOptions( smoothMeshOptions );
    }

    // get the base mesh (without vertex velocities)
    if( isSmooth ) {
        FF_LOG( debug ) << "Generating smoothed mesh from original Maya mesh.\n";
        MObject smoothBaseMeshObj = baseMesh.generateSmoothMesh( parentObject, &smoothMeshOptions );
        MFnMesh smoothBaseMesh( smoothBaseMeshObj );
        copy_maya_mesh_internal( smoothBaseMesh, outMesh, generateNormals, generateUVCoords, generateColors );
    } else {
        copy_maya_mesh_internal( baseMesh, outMesh, generateNormals, generateUVCoords, generateColors );
    }

    FF_LOG( debug ) << "Retrieved a mesh that has " << outMesh.vertex_count() << " vertices and "
                    << outMesh.face_count() << " faces.\n";

    // generate the vertex velocities if requested
    if( generateVelocity ) {

        MTime currentTime = MAnimControl::currentTime(); // may want to change this to pass it in.

        // currently I'm subdividing the time offset up to 50 times to try to find a mesh of the same topology. The
        // number 50 is arbitrary. I'm not starting at a half frame offset because in the 3dsmax world, the topology
        // changes at the +0.5 frame mark. Maya does not appear to do this. Maya appears to allow changing topology at
        // any given time, thus this is not so important. There are plenty of cases where the +0.49 offset does not
        // work.
        bool successfullyCreatedVelocities = false;
        float currentOffset = 0.49f;
        for( int j = 0; j < 50 && !successfullyCreatedVelocities; ++j ) {

            MTime offsetTime( currentTime.value() + currentOffset );
            MDGContext offsetTimeContext( offsetTime );

            // The time values in Maya are not floating-point precise. We will eventually meet our current time in this
            // loop. If we do, we have to exit the loop.
            if( offsetTime.value() == currentTime.value() )
                break;

            // Get the object (mesh) from the plug
            FF_LOG( debug ) << "Retrieving the mesh at time " << offsetTime.value()
                            << " to attempt to match vertices with mesh generated at time " << currentTime.value()
                            << " to create vertex velocities.\n";
            MObject offsetMeshObj;
            inPlug.getValue( offsetMeshObj, offsetTimeContext );

            // Cast the object as a mesh (because it is a mesh).
            if( !offsetMeshObj.hasFn( MFn::kMesh ) )
                throw std::runtime_error( "copy_maya_mesh error: Could not generate vertex velocities." );
            MFnMesh offsetMesh( offsetMeshObj, &status );
            if( !status )
                throw std::runtime_error( "copy_maya_mesh error: Could not generate vertex velocities." );

            // Call our function that generates vertex velocities.
            // This can be done on either the retrieved mesh, or on a smoothed mesh version (if requested).
            // The function to create vertex velocities will return false if it did not work at this time step. In that
            // case, we have to subdivide the time step and try again. It may not work at this time step because the
            // vertex count may differ, or the face array is not identical to the mesh at the current time.
            if( isSmooth ) {
                FF_LOG( debug ) << "Generating smoothed mesh for velocities from original Maya mesh.\n";
                MObject offsetSmoothMeshObj = offsetMesh.generateSmoothMesh( parentObject, &smoothMeshOptions );
                MFnMesh offsetSmoothMesh( offsetSmoothMeshObj );
                successfullyCreatedVelocities = generate_vertex_velocities( offsetSmoothMesh, outMesh, currentOffset );
            } else {
                successfullyCreatedVelocities = generate_vertex_velocities( offsetMesh, outMesh, currentOffset );
            }

            // Subdivide our time offset if we were not successful in creating velocities at the current offset.
            currentOffset *= 0.5f;
        }

        if( !successfullyCreatedVelocities ) {
            FF_LOG( debug ) << "Could not create velocities for maya mesh: \"" << baseMesh.name().asChar() << "\"\n";
        }
    }
}

void mesh_copy( MObject parentOrOwner, const frantic::geometry::trimesh3& mesh ) {
    mesh_copy_time_offset( parentOrOwner, mesh, 0.f );
}

void mesh_copy_time_offset( MObject parentOrOwner, const frantic::geometry::trimesh3& mesh, float timeOffset ) {
    MStatus stat;

    MFnMesh fnMesh;
    fnMesh.setCheckSamePointTwice( false );

    MFloatPointArray vertexArray;
    MIntArray polygonCounts;
    MIntArray polygonConnects;

    copy_mesh_geometry( vertexArray, mesh );

    if( timeOffset && mesh.has_vertex_channel( _T("Velocity") ) ) {
        apply_velocity_offset( vertexArray, mesh, timeOffset );
    }

    copy_mesh_topology( polygonCounts, polygonConnects, mesh );

    MObject meshData = fnMesh.create( vertexArray.length(), polygonCounts.length(), vertexArray, polygonCounts,
                                      polygonConnects, parentOrOwner, &stat );
    if( !stat ) {
        throw std::runtime_error( "mesh_copy Error: unable to create mesh data: " +
                                  std::string( stat.errorString().asChar() ) );
    }

    const frantic::tstring normal( _T( "Normal" ) );
    if( mesh.has_vertex_channel( normal ) ) {
        copy_mesh_normals( fnMesh, mesh );
    }

    const frantic::tstring color( _T("Color") );
    if( mesh.has_vertex_channel( color ) ) {
        copy_mesh_color( fnMesh, _T( "colorPV" ), mesh, color );
    }

    const frantic::tstring textureCoord( _T("TextureCoord") );
    if( mesh.has_vertex_channel( textureCoord ) ) {
        copy_mesh_texture_coord( fnMesh, mesh, textureCoord );
    }

    const frantic::tstring velocity( _T("Velocity") );
    if( mesh.has_vertex_channel( velocity ) ) {
        copy_mesh_color( fnMesh, _T( "velocityPV" ), mesh, velocity, scale_color_transform( 1.0 / get_fps() ) );
    }
}

} // namespace geometry
} // namespace maya
} // namespace frantic
