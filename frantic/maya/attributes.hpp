// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MAngle.h>
#include <maya/MDGContext.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>

#include <frantic/graphics/color3f.hpp>

#include <boost/lexical_cast.hpp>

namespace frantic {
namespace maya {

inline float get_float_attribute( const MFnDependencyNode& node, const MString& attribute,
                                  const MDGContext& context = MDGContext::fsNormal, MStatus* outStatus = NULL ) {
    MPlug plug = node.findPlug( attribute );
    return plug.asFloat( const_cast<MDGContext&>( context ), outStatus );
}

inline int get_int_attribute( const MFnDependencyNode& node, const MString& attribute,
                              const MDGContext& context = MDGContext::fsNormal, MStatus* outStatus = NULL ) {
    MPlug plug = node.findPlug( attribute );
    return plug.asInt( const_cast<MDGContext&>( context ), outStatus );
}

inline MString get_string_attribute( const MFnDependencyNode& node, const MString& attribute,
                                     const MDGContext& context = MDGContext::fsNormal, MStatus* outStatus = NULL ) {
    MPlug plug = node.findPlug( attribute );
    return plug.asString( const_cast<MDGContext&>( context ), outStatus );
}

/**
 * Gets an enum attribute as a string.  The implementation is a little weird because this is a feature that is only
 * available in Mel.
 */
inline MString get_enum_attribute( const MFnDependencyNode& node, const MString& attribute,
                                   const MDGContext& context = MDGContext::fsNormal, MStatus* outStatus = NULL ) {
    MString result;
    MTime currentTime;
    context.getTime( currentTime );
    MStatus returnState = MGlobal::executeCommand(
        MString( "getAttr -asString -time " ) +
            MString( boost::lexical_cast<std::string>( currentTime.as( MTime::uiUnit() ) ).c_str() ) +
            MString( " \"" ) + node.name() + MString( "." ) + attribute + MString( "\";" ),
        result );

    if( outStatus != NULL )
        *outStatus = returnState;

    return result;
}

inline bool get_boolean_attribute( const MFnDependencyNode& node, const MString& attribute,
                                   const MDGContext& context = MDGContext::fsNormal, MStatus* outStatus = NULL ) {
    return get_int_attribute( node, attribute, context, outStatus ) != 0;
}

/**
 * Gets a color attribute group.  Due to the way in which color attributes must be stored, its better just to do the
 * frantic type conversion within this method
 */
inline frantic::graphics::color3f get_color_attribute( const MFnDependencyNode& node, const MString& attribute,
                                                       const MDGContext& context = MDGContext::fsNormal ) {
    MPlug plug = node.findPlug( attribute );
    frantic::graphics::color3f result;

    // Unfortunately maya is a little sketchy on whether this is even a reasonable assumption to make for color
    // attributes
    if( plug.numChildren() == 3 ) {
        result.r = plug.child( 0 ).asFloat( const_cast<MDGContext&>( context ) );
        result.g = plug.child( 1 ).asFloat( const_cast<MDGContext&>( context ) );
        result.b = plug.child( 2 ).asFloat( const_cast<MDGContext&>( context ) );
    }

    return result;
}

/**
 * It is _very_ important that you use this method, and not 'get_float_attribute' to grab angle attributes, as the other
 * method will just return 0.
 */
inline MAngle get_angle_attribute( const MFnDependencyNode& node, const MString& attribute,
                                   const MDGContext& context = MDGContext::fsNormal ) {
    MPlug plug = node.findPlug( attribute );
    return plug.asMAngle( const_cast<MDGContext&>( context ) );
}

} // namespace maya
} // namespace frantic
