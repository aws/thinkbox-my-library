// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/maya/PRTObject_base.hpp>
#include <maya/MFnParticleSystem.h>
#include <maya/MPxNode.h>

namespace frantic {
namespace maya {

class PRTMayaParticle : public MPxNode, public PRTObjectBase {
  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    static const MString typeName;

    static const MString inParticleAttribute;

  private:
    // This attribute is meant to connect to the maya particle's positions attribute itself so we know where we are
    // getting our particles from. The value/attribute itself can be ignored
    static MObject inConnect;

    // Output particles
    static MObject outParticleStream;

  public:
    PRTMayaParticle();
    virtual ~PRTMayaParticle();
    virtual void postConstructor();
    virtual MStatus compute( const MPlug& plug, MDataBlock& block );

    virtual frantic::particles::streams::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& objectTransform, const MDGContext& context ) const {
        return getParticleStream( objectTransform, context, false );
    }

    virtual frantic::particles::streams::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& objectTransform,
                               const MDGContext& context ) const {
        return getParticleStream( objectTransform, context, true );
    }

    frantic::particles::streams::particle_istream_ptr
    getParticleStream( const frantic::graphics::transform4f& objectTransform, const MDGContext& context,
                       bool isViewport ) const;

    MObject getConnectedMayaParticleStream( MStatus* status = NULL ) const;

  public:
    /**
     * Retrieves the PRT Wrapper from the given maya particle system
     * Does not check for deformed or have support for auto wrapper creation and relinking
     * @param particleStream Stream to get from
     */
    static MObject getPRTMayaParticleFromMayaParticleStream( const MFnParticleSystem& particleStream,
                                                             MStatus* status = NULL );

    /**
     * Retrieves the PRT Wrapper from the given maya particle system.  This checks for the deformed version of the
     * particle stream and updates out of date connections if needed
     * @param particleStream Stream to get from
     * @param autoCreate True to autocreate the wrapper if it does not exist
     */
    static MObject getPRTMayaParticleFromMayaParticleStreamCheckDeformed( const MFnParticleSystem& particleStream,
                                                                          MStatus* status = NULL,
                                                                          bool autoCreate = true );

    ///**
    // * Move this to maya_util?
    // * Checks if the particle stream has a deformed version
    // */
    // static bool mayaParticleStreamHasDeformed( const MFnParticleSystem& particleStream, MStatus* status = NULL );
    //
    ///**
    // * Move this to maya_util?
    // * Checks if the particle stream has an original version
    // */
    // static bool mayaParticleStreamHasOriginal( const MFnParticleSystem& particleStream, MStatus* status = NULL );
};

} // namespace maya
} // namespace frantic
