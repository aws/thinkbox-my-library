// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "stdafx.h"

#include <frantic/maya/animation/animation_data.hpp>

#include <maya/MDGModifier.h>
#include <maya/MPlugArray.h>

namespace frantic {
namespace maya {
namespace animation {

bool animation_data::animation_keyframe_data::loadFromCurve( const MFnAnimCurve& curve, unsigned int index ) {
    MStatus status;

    bool ok = true;
    m_time = curve.time( index, &status );
    if( status != MS::kSuccess )
        ok = false;

    m_value = curve.value( index, &status );
    if( status != MS::kSuccess )
        ok = false;

    status = curve.getTangent( index, m_inTangent.m_angle, m_inTangent.m_weight, true );
    if( status != MS::kSuccess )
        ok = false;

    status = curve.getTangent( index, m_outTangent.m_angle, m_outTangent.m_weight, false );
    if( status != MS::kSuccess )
        ok = false;

    m_inTangent.m_tangentType = curve.inTangentType( index, &status );
    if( status != MS::kSuccess )
        ok = false;

    m_outTangent.m_tangentType = curve.outTangentType( index, &status );
    if( status != MS::kSuccess )
        ok = false;

    return ok;
}

bool animation_data::animation_keyframe_data::addToCurve( MFnAnimCurve& curve ) const {
    MStatus status;

    int index = curve.addKey( m_time, m_value, m_inTangent.m_tangentType, m_outTangent.m_tangentType, NULL, &status );
    if( status != MS::kSuccess )
        return false;

    status = curve.setTangentsLocked( index, false );

    status = curve.setTangent( index, m_inTangent.m_angle, m_inTangent.m_weight, true );
    if( status != MS::kSuccess )
        return false;

    status = curve.setTangent( index, m_outTangent.m_angle, m_outTangent.m_weight, false );
    if( status != MS::kSuccess )
        return false;

    // Setting the weight and angle changes the tangent type.  Reset it just in case.
    status = curve.setInTangentType( index, m_inTangent.m_tangentType );
    status = curve.setOutTangentType( index, m_outTangent.m_tangentType );

    status = curve.setTangentsLocked( index, true );

    return true;
}

/**
 * Checks if the given attribute has an animation keyframe associated with it and returns that object
 * Object will be used for MFnAnimCurve's constructor
 */
bool animation_data::get_animation( const MPlug& attribute, MObject& outObject ) {
    MStatus status;

    MPlugArray connections;
    attribute.connectedTo( connections, true, false, &status );
    if( status != MS::kSuccess )
        return false;

    for( unsigned int i = 0; i < connections.length(); i++ ) {
        MObject connected = connections[i].node();
        if( connected.hasFn( MFn::kAnimCurve ) ) {
            outObject = connected;
            return true;
        }
    }

    outObject = MObject::kNullObj;
    return false;
}

/**
 * Checks if the given attribute has an animation keyframe associated with it
 */
bool animation_data::has_animation( const MPlug& attribute ) {
    MObject tmp;
    bool ok = get_animation( attribute, tmp );
    return ok;
}

animation_data::animation_data()
    : m_animCurveType( MFnAnimCurve::kAnimCurveUnknown )
    , m_preInfinityType( MFnAnimCurve::kConstant )
    , m_postInfinityType( MFnAnimCurve::kConstant )
    , m_keyframes()
    , m_weighted( false ) {}

animation_data::~animation_data() {}

bool animation_data::loadFromCurve( const MPlug& curve ) {
    MObject curveObj;
    bool ok = get_animation( curve, curveObj );
    if( !ok )
        return false;

    MStatus status;
    MFnAnimCurve animCurve( curveObj, &status );
    if( status != MS::kSuccess )
        return false;

    return loadFromCurve( animCurve );
}

bool animation_data::applyToCurve( MPlug& curve ) const {
    MStatus status;
    MFnAnimCurve fnAnimCurve;

    // Check if it's already there
    MObject curveObj;
    bool exists = get_animation( curve, curveObj );

    // Not there, create it
    if( !exists ) {
        // No animation info, we're done
        if( this->isEmpty() )
            return true;

        // Make sure we can animate this
        status = curve.setKeyable( true );
        if( status != MS::kSuccess )
            return false;

        MFnAnimCurve fnAnimCurve;
        curveObj = fnAnimCurve.create( curve, m_animCurveType, NULL, &status );
        if( status != MS::kSuccess )
            return false;

        return applyToCurve( fnAnimCurve );
    }

    MFnAnimCurve animCurve( curveObj, &status );
    if( status != MS::kSuccess )
        return false;

    return applyToCurve( animCurve );
}

bool animation_data::loadFromCurve( const MFnAnimCurve& curve ) {
    bool ok = true;
    MStatus status;
    m_keyframes.clear();

    m_animCurveType = curve.animCurveType( &status );
    if( status != MS::kSuccess )
        ok = false;

    m_preInfinityType = curve.preInfinityType( &status );
    if( status != MS::kSuccess )
        ok = false;

    m_postInfinityType = curve.postInfinityType( &status );
    if( status != MS::kSuccess )
        ok = false;

    m_weighted = curve.isWeighted( &status );
    if( status != MS::kSuccess )
        ok = false;

    for( unsigned int i = 0; i < curve.numKeys( &status ); i++ ) {
        animation_keyframe_data currentKey;
        bool check = currentKey.loadFromCurve( curve, i );
        if( !check )
            ok = false;
        m_keyframes.push_back( currentKey );
    }

    return ok;
}

bool animation_data::applyToCurve( MFnAnimCurve& curve ) const {
    MStatus status;

    status = curve.setPreInfinityType( m_preInfinityType );
    if( status != MS::kSuccess )
        return false;

    status = curve.setPostInfinityType( m_postInfinityType );
    if( status != MS::kSuccess )
        return false;

    status = curve.setIsWeighted( m_weighted );
    if( status != MS::kSuccess )
        return false;

    // Clear all the keys first ...
    while( curve.numKeys( &status ) > 0 ) {
        if( status != MS::kSuccess )
            return false;

        status = curve.remove( curve.numKeys() - 1 );
        if( status != MS::kSuccess )
            return false;
    }

    // ... then add our own
    for( std::vector<animation_keyframe_data>::const_iterator iter = m_keyframes.begin(); iter != m_keyframes.end();
         ++iter ) {
        bool check = iter->addToCurve( curve );
        if( !check )
            return false;
    }

    return true;
}

void animation_data::clearKeyFrames() { m_keyframes.clear(); }

bool animation_data::isEmpty() const { return m_keyframes.empty(); }

} // namespace animation
} // namespace maya
} // namespace frantic
