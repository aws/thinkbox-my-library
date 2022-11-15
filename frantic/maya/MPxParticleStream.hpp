// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MDGContext.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MPxData.h>

#include <frantic/maya/PRTObject_base.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

namespace frantic {
namespace maya {

class MPxParticleStream : public MPxData {

  public:
    static const MString typeName;
    static const MTypeId id;
    static void* creator();

  public:
    virtual ~MPxParticleStream(){};

    virtual void copy( const MPxData& src ) = 0;

    virtual MTypeId typeId() const = 0;

    virtual MString name() const = 0;

    virtual frantic::particles::streams::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& objectSpace,
                             const MDGContext& context = MDGContext::fsNormal ) const = 0;

    virtual frantic::particles::streams::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                               const MDGContext& context = MDGContext::fsNormal ) const = 0;

    virtual unsigned long getVersion() const = 0;

    virtual void setParticleSource( particle_stream_source* prtObj ) = 0;

    virtual particle_stream_source* getParticleSource() const = 0;
};

} // namespace maya
} // namespace frantic
