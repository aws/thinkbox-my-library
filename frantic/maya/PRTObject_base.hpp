// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MDGContext.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>

#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

#include <boost/shared_ptr.hpp>
#include <vector>

namespace frantic {
namespace maya {

//////////////////////////////////////////////////////////////////////////////////////////////////////

class particle_stream_source {

  public:
    virtual ~particle_stream_source() {}

    virtual frantic::particles::streams::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& objectSpace,
                             const MDGContext& context = MDGContext::fsNormal ) const = 0;

    virtual frantic::particles::streams::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                               const MDGContext& context = MDGContext::fsNormal ) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

class PRTObjectBase : public particle_stream_source {

  public:
    typedef frantic::particles::streams::particle_istream_ptr particle_istream_ptr;

    /**
     * Returns the particle stream to use in a full-scale render
     * @param context specify the evaluation context.  Defaults to the current context
     */
    virtual particle_istream_ptr getRenderParticleStream( const frantic::graphics::transform4f& objectSpace,
                                                          const MDGContext& context = MDGContext::fsNormal ) const = 0;

    /**
     * Returns the particle stream to use in for the viewport render
     * @param context specify the evaluation context.  Defaults to the current context
     */
    virtual particle_istream_ptr getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                                                            const MDGContext& context = MDGContext::fsNormal ) const;

  public:
    /**
     * Gets the final render or viewport particle stream taking into account additional transformations to be applied to
     * the particle stream
     */
    static particle_istream_ptr getFinalParticleStream( const MFnDependencyNode& depNode,
                                                        const frantic::graphics::transform4f& objectSpace,
                                                        const MDGContext& context = MDGContext::fsNormal,
                                                        bool isViewport = false,
                                                        MString outParticleStreamAttr = "outParticleStream" );

    /**
     * Helper method to get the particle stream from the MPxData object
     */
    static particle_istream_ptr getParticleStreamFromMPxData( const MFnDependencyNode& depNode,
                                                              const frantic::graphics::transform4f& objectSpace,
                                                              const MDGContext& context = MDGContext::fsNormal,
                                                              bool isViewport = false,
                                                              MString outParticleStreamAttr = "outParticleStream" );

    /**
     * Helper method to iterate to the final dependency node in the particle stream chain
     */
    static MObject getEndOfStreamChain( const MFnDependencyNode& depNode,
                                        MString outParticleStreamAttr = "outParticleStream" );

    /**
     * Helper method to get the next element in the chain.  Return kNullObj if we walk off the end.
     */
    static MObject nextElementInChain( const MFnDependencyNode& depNode,
                                       MString outParticleStreamAttr = "outParticleStream" );

    /**
     * Helper method to get the previous element in the chain.  Return kNullObj if we walk off the end.
     */
    static MObject previousElementInChain( const MFnDependencyNode& depNode,
                                           MString inParticleStreamAttr = "inParticleStream" );

    /**
     * Helper method to check if the dependency node has the given particle stream attribute
     */
    static bool hasParticleStreamMPxData( const MFnDependencyNode& depNode,
                                          MString outParticleStreamAttr = "outParticleStream" );
};

} // namespace maya
} // namespace frantic
