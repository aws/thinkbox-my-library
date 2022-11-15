// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MDGContext.h>
#include <maya/MGlobal.h>
#include <maya/MPxData.h>
#include <maya/MRenderView.h>
#include <maya/MTime.h>

#include <frantic/graphics/units.hpp>

namespace frantic {
namespace maya {

inline double get_fps() { return MTime( 1.0, MTime::kSeconds ).as( MTime::uiUnit() ); }

inline double get_scale_to_meters() {
    MString units;
    MGlobal::executeCommand( "currentUnit -q;", units );
    if( units == "mm" )
        return 0.001;
    if( units == "cm" )
        return 0.01;
    if( units == "m" )
        return 1.0;
    if( units == "in" )
        return 0.0254;
    if( units == "ft" )
        return 0.3048;
    if( units == "yd" )
        return 0.9144;
    else
        return 0.0;
}

inline frantic::graphics::coordinate_system::option get_coordinate_system() {
    MString upAxis;
    MGlobal::executeCommand( "upAxis -q -axis;", upAxis );
    if( upAxis == "y" )
        return frantic::graphics::coordinate_system::right_handed_yup;
    else
        return frantic::graphics::coordinate_system::right_handed_zup;
}

inline bool is_batch_mode() {
    // this is one of at least three ways of detecting if you are in batch (i.e. non-ui) mode, however the other
    // two involve calling mel
    return !MRenderView::doesRenderEditorExist();
}

/**
 * Grab the 'worldMatrix' of the object at the specified dag path at the specified time.  The full dag path is required
 * to actually get a proper transform, since just an MObject can appear multiple times in the same scene under different
 * transforms.
 *
 * @param dagNodePath the path to the scene object
 * @param currentTime the scene time at which to get the transform
 * @param outTransform location where the world transform will be placed when retrieved
 * @return whether or not the world matrix could be retrieved
 */
bool get_object_world_matrix( const MDagPath& dagNodePath, const MDGContext& currentContext,
                              frantic::graphics::transform4f& outTransform );

/**
 * Change the cursor to an hourglass/waiting cursor while in scope.
 */
class scoped_wait_cursor {
    bool m_setWaitCursor;

  public:
    scoped_wait_cursor();
    ~scoped_wait_cursor();
};

// dynamic_cast seems to fail on OS X Maya 2015 and 2016 for these types of conversions.
// It's possible it's due to us using a different compiler than Autodesk and the vtables being incompatible.
// This is a safe (...probably) workaround.
template <typename T>
T mpx_cast( typename boost::conditional<boost::is_const<typename boost::remove_pointer<T>::type>::value, const MPxData*,
                                        MPxData*>::type data ) {
    if( !data ) {
        return NULL;
    }

    T result = dynamic_cast<T>( data );
    if( !result &&
        data->typeId().id() == boost::remove_pointer<typename boost::remove_const<T>::type>::type::id.id() ) {
        return static_cast<T>( data );
    }

    return result;
}

} // namespace maya
} // namespace frantic
