// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MFnAnimCurve.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>

namespace frantic {
namespace maya {
namespace animation {

// stores maya key frame data
struct animation_data {

    struct animation_tangent_data {
        MFnAnimCurve::TangentType m_tangentType;
        MAngle m_angle;
        double m_weight;

        animation_tangent_data()
            : m_angle( 0 )
            , m_weight( 1.0 )
            , m_tangentType(
#if MAYA_API_VERSION > 201100
                  MFnAnimCurve::kTangentAuto
#else
                  MFnAnimCurve::kTangentGlobal
#endif
              ) {
        }
    };

    struct animation_keyframe_data {
        animation_tangent_data m_inTangent;
        animation_tangent_data m_outTangent;

        MTime m_time;
        double m_value;

        animation_keyframe_data()
            : m_time()
            , m_value( 0 )
            , m_inTangent()
            , m_outTangent() {}

        bool loadFromCurve( const MFnAnimCurve& curve, unsigned int index );
        bool addToCurve( MFnAnimCurve& curve ) const;
    };

    MFnAnimCurve::AnimCurveType m_animCurveType;
    MFnAnimCurve::InfinityType m_preInfinityType;
    MFnAnimCurve::InfinityType m_postInfinityType;
    bool m_weighted;
    std::vector<animation_keyframe_data> m_keyframes;

  public:
    animation_data();
    ~animation_data();

    bool loadFromCurve( const MPlug& curve );
    bool loadFromCurve( const MFnAnimCurve& curve );
    bool applyToCurve( MPlug& curve ) const;
    bool applyToCurve( MFnAnimCurve& curve ) const;
    void clearKeyFrames();

    bool isEmpty() const;

  public:
    static bool get_animation( const MPlug& attribute, MObject& outObject );
    static bool has_animation( const MPlug& attribute );
};

struct animation_data_vector3 {
    animation_data value[3];

    animation_data_vector3() {}

    animation_data_vector3( const animation_data& xAnim, const animation_data& yAnim, const animation_data& zAnim ) {
        x() = xAnim;
        y() = yAnim;
        z() = zAnim;
    }

    animation_data& x() { return value[0]; }
    animation_data& y() { return value[1]; }
    animation_data& z() { return value[2]; }

    const animation_data& x() const { return value[0]; }
    const animation_data& y() const { return value[1]; }
    const animation_data& z() const { return value[2]; }

    bool isEmpty() const { return x().isEmpty() && y().isEmpty() && z().isEmpty(); }
};

struct animation_data_vector4 {
    animation_data value[4];

    animation_data_vector4() {}

    animation_data_vector4( const animation_data& real, const animation_data_vector3& imag ) {
        w() = real;
        x() = imag.x();
        y() = imag.y();
        z() = imag.z();
    }

    animation_data_vector4( const animation_data& wAnim, const animation_data& xAnim, const animation_data& yAnim,
                            const animation_data& zAnim ) {
        w() = wAnim;
        x() = xAnim;
        y() = yAnim;
        z() = zAnim;
    }

    animation_data& w() { return value[0]; }
    animation_data& x() { return value[1]; }
    animation_data& y() { return value[2]; }
    animation_data& z() { return value[3]; }

    const animation_data& w() const { return value[0]; }
    const animation_data& x() const { return value[1]; }
    const animation_data& y() const { return value[2]; }
    const animation_data& z() const { return value[3]; }

    animation_data getReal() const { return animation_data( x() ); }

    animation_data_vector3 getImaginary() const { return animation_data_vector3( x(), y(), z() ); }

    bool isEmpty() const { return w().isEmpty() && x().isEmpty() && y().isEmpty() && z().isEmpty(); }
};

} // namespace animation
} // namespace maya
} // namespace frantic
