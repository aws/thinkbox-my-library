// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MBoundingBox.h>
#include <maya/MColor.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatVector.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include <frantic/graphics/boundbox3f.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/strings/tstring.hpp>

namespace frantic {
namespace maya {

inline frantic::tstring from_maya_t( const MString& mayaString ) {
    return frantic::strings::to_tstring( mayaString.asChar() );
}

inline MString to_maya_t( const frantic::tstring& tString ) {
    return MString( frantic::strings::to_string( tString ).c_str() );
}

inline frantic::graphics::vector3f from_maya_t( const MFloatVector& mayaVector ) {
    return frantic::graphics::vector3f( mayaVector.x, mayaVector.y, mayaVector.z );
}

inline MFloatVector to_maya_t( const frantic::graphics::vector3f& vector ) {
    return MFloatVector( vector.x, vector.y, vector.z );
}

inline frantic::graphics::color3f from_maya_t( const MColor mayaColor ) {
    return frantic::graphics::color3f( mayaColor.r, mayaColor.g, mayaColor.b );
}

inline MColor to_maya_t( const frantic::graphics::color3f& color ) { return MColor( color.r, color.g, color.b ); }

inline frantic::graphics::boundbox3f from_maya_t( const MBoundingBox& bounds ) {
    using namespace frantic::graphics;
    const vector3f minPoint( from_maya_t( bounds.min() ) );
    const vector3f maxPoint( from_maya_t( bounds.max() ) );
    return boundbox3f( minPoint, maxPoint );
}

inline MBoundingBox to_maya_t( const frantic::graphics::boundbox3f& bounds ) {
    const MPoint minPoint( to_maya_t( bounds.minimum() ) );
    const MPoint maxPoint( to_maya_t( bounds.maximum() ) );
    return MBoundingBox( minPoint, maxPoint );
}

inline frantic::graphics::transform4f from_maya_t( const MFloatMatrix& mayaMatrix ) {
    return frantic::graphics::transform4f(
        mayaMatrix( 0, 0 ), mayaMatrix( 0, 1 ), mayaMatrix( 0, 2 ), mayaMatrix( 0, 3 ), mayaMatrix( 1, 0 ),
        mayaMatrix( 1, 1 ), mayaMatrix( 1, 2 ), mayaMatrix( 1, 3 ), mayaMatrix( 2, 0 ), mayaMatrix( 2, 1 ),
        mayaMatrix( 2, 2 ), mayaMatrix( 2, 3 ), mayaMatrix( 3, 0 ), mayaMatrix( 3, 1 ), mayaMatrix( 3, 2 ),
        mayaMatrix( 3, 3 ) );
}

inline frantic::graphics::transform4f from_maya_t( const MMatrix& mayaMatrix ) {
    return frantic::graphics::transform4f(
        (float)mayaMatrix( 0, 0 ), (float)mayaMatrix( 0, 1 ), (float)mayaMatrix( 0, 2 ), (float)mayaMatrix( 0, 3 ),
        (float)mayaMatrix( 1, 0 ), (float)mayaMatrix( 1, 1 ), (float)mayaMatrix( 1, 2 ), (float)mayaMatrix( 1, 3 ),
        (float)mayaMatrix( 2, 0 ), (float)mayaMatrix( 2, 1 ), (float)mayaMatrix( 2, 2 ), (float)mayaMatrix( 2, 3 ),
        (float)mayaMatrix( 3, 0 ), (float)mayaMatrix( 3, 1 ), (float)mayaMatrix( 3, 2 ), (float)mayaMatrix( 3, 3 ) );
}

inline MFloatMatrix to_maya_t( const frantic::graphics::transform4f& matrix ) {
    MFloatMatrix outMatrix;
    outMatrix( 0, 0 ) = matrix[0];
    outMatrix( 0, 1 ) = matrix[1];
    outMatrix( 0, 2 ) = matrix[2];
    outMatrix( 0, 3 ) = matrix[3];
    outMatrix( 1, 0 ) = matrix[0];
    outMatrix( 1, 1 ) = matrix[1];
    outMatrix( 1, 2 ) = matrix[2];
    outMatrix( 1, 3 ) = matrix[3];
    outMatrix( 2, 0 ) = matrix[0];
    outMatrix( 2, 1 ) = matrix[1];
    outMatrix( 2, 2 ) = matrix[2];
    outMatrix( 2, 3 ) = matrix[3];
    outMatrix( 3, 0 ) = matrix[0];
    outMatrix( 3, 1 ) = matrix[1];
    outMatrix( 3, 2 ) = matrix[2];
    outMatrix( 3, 3 ) = matrix[3];
    return outMatrix;
}

inline MStringArray to_maya_t( const std::vector<frantic::tstring>& tStringList ) {
    MStringArray result;
    for( std::vector<frantic::tstring>::const_iterator iter = tStringList.begin(); iter != tStringList.end(); ++iter ) {
        MString current = to_maya_t( *iter );
        result.append( current );
    }
    return result;
}

inline std::vector<frantic::tstring> from_maya_t( const MStringArray& mayaStringList ) {
    std::vector<frantic::tstring> result;
    for( unsigned int i = 0; i < mayaStringList.length(); i++ ) {
        MString current = mayaStringList[i];
        result.push_back( from_maya_t( current ) );
    }
    return result;
}

inline std::vector<int> from_maya_t( const MIntArray& mayaIntList ) {
    std::vector<int> result;
    for( unsigned int i = 0; i < mayaIntList.length(); i++ ) {
        int current = mayaIntList[i];
        result.push_back( current );
    }
    return result;
}

} // namespace maya
} // namespace frantic
