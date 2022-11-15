// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/PRTObject_base.hpp>
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/util.hpp>

#include <maya/MFnPluginData.h>
#include <maya/MPlugArray.h>

namespace frantic {
namespace maya {

PRTObjectBase::particle_istream_ptr
PRTObjectBase::getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                                          const MDGContext& context ) const {
    return getRenderParticleStream( objectSpace, context );
}

PRTObjectBase::particle_istream_ptr
PRTObjectBase::getFinalParticleStream( const MFnDependencyNode& depNode,
                                       const frantic::graphics::transform4f& objectSpace, const MDGContext& context,
                                       bool isViewport, MString outParticleStreamAttr ) {
    MStatus stat;

    // Get the dependency node at the end of the chain
    MObject finalNode = getEndOfStreamChain( depNode, outParticleStreamAttr );
    if( stat != MStatus::kSuccess )
        throw std::runtime_error( ( "DEBUG: could not find end of chain using attribute '" + outParticleStreamAttr +
                                    "' from depNode '" + depNode.name() + "': " + stat.errorString() )
                                      .asChar() );

    // Extract the final Particle Stream Data
    return getParticleStreamFromMPxData( finalNode, objectSpace, context, isViewport, outParticleStreamAttr );
}

PRTObjectBase::particle_istream_ptr PRTObjectBase::getParticleStreamFromMPxData(
    const MFnDependencyNode& depNode, const frantic::graphics::transform4f& objectSpace, const MDGContext& context,
    bool isViewport, MString outParticleStreamAttr ) {
    MStatus stat;

    MPlug plug = depNode.findPlug( outParticleStreamAttr, &stat );
    if( stat != MStatus::kSuccess )
        throw std::runtime_error( ( "DEBUG: could not find plug '" + outParticleStreamAttr + "' from depNode '" +
                                    depNode.name() + "': " + stat.errorString() )
                                      .asChar() );

    MObject prtMpxData;
    plug.getValue( prtMpxData );
    MFnPluginData fnData( prtMpxData );
    MPxParticleStream* streamMPxData = frantic::maya::mpx_cast<MPxParticleStream*>( fnData.data( &stat ) );

    if( stat != MStatus::kSuccess || streamMPxData == NULL )
        throw std::runtime_error( ( "DEBUG: could not get MPxParticleStream from '" + outParticleStreamAttr +
                                    "' from depNode '" + depNode.name() + "': " + stat.errorString() )
                                      .asChar() );

    frantic::particles::streams::particle_istream_ptr outStream;
    if( isViewport )
        outStream = streamMPxData->getViewportParticleStream( objectSpace, context );
    else
        outStream = streamMPxData->getRenderParticleStream( objectSpace, context );

    return outStream;
}

MObject PRTObjectBase::getEndOfStreamChain( const MFnDependencyNode& depNode, MString outParticleStreamAttr ) {
    MStatus stat;

    MPlug streamPlug = depNode.findPlug( outParticleStreamAttr, &stat );
    if( stat != MStatus::kSuccess ) {
        return depNode.object();
    }

    // Traverse the connections graph
    MPlugArray plugs;
    streamPlug.connectedTo( plugs, false, true );
    while( plugs.length() > 0 ) {
        MPlug nextPlug;

        for( std::size_t i = 0; i < plugs.length(); ++i ) {
            MObject currentObject = plugs[i].node( &stat );
            MFnDependencyNode nextDepNode( currentObject, &stat );

            if( stat != MStatus::kSuccess ) {
                // The object isn't a dependency node, ignore it.
                continue;
            }

            nextPlug = nextDepNode.findPlug( outParticleStreamAttr, &stat );

            if( stat == MStatus::kSuccess ) {
                // We found a dependency node with the required attribute,
                // we can probably ignore the rest at this level.
                break;
            }
        }

        // We only want to continue here if we actually found a new node with the required attribute
        // at this level, otherwise, we're done.
        if( stat == MStatus::kSuccess ) {
            streamPlug = nextPlug;
            streamPlug.connectedTo( plugs, false, true );
        } else {
            return streamPlug.node();
        }
    }

    return streamPlug.node();
}

MObject PRTObjectBase::nextElementInChain( const MFnDependencyNode& depNode, MString outParticleStreamAttr ) {
    MStatus stat;

    MPlug streamPlug = depNode.findPlug( outParticleStreamAttr, &stat );
    if( stat != MStatus::kSuccess ) {
        return MObject::kNullObj;
    }

    // Traverse the connections graph
    MPlugArray plugs;
    streamPlug.connectedTo( plugs, false, true );
    if( plugs.length() > 0 ) {
        MObject currentObject = plugs[0].node( &stat );
        MFnDependencyNode nextDepNode( currentObject, &stat );
        if( stat == MStatus::kSuccess )
            return currentObject;
    }

    return MObject::kNullObj;
}

MObject PRTObjectBase::previousElementInChain( const MFnDependencyNode& depNode, MString inParticleStreamAttr ) {
    MStatus stat;

    MPlug streamPlug = depNode.findPlug( inParticleStreamAttr, &stat );
    if( stat != MStatus::kSuccess ) {
        return MObject::kNullObj;
    }

    // Traverse the connections graph
    MPlugArray plugs;
    streamPlug.connectedTo( plugs, true, false );
    if( plugs.length() > 0 ) {
        MObject currentObject = plugs[0].node( &stat );
        MFnDependencyNode nextDepNode( currentObject, &stat );
        if( stat == MStatus::kSuccess )
            return currentObject;
    }

    return MObject::kNullObj;
}

bool PRTObjectBase::hasParticleStreamMPxData( const MFnDependencyNode& depNode, MString outParticleStreamAttr ) {
    MStatus stat;

    MPlug plug = depNode.findPlug( outParticleStreamAttr, &stat );
    if( stat != MStatus::kSuccess )
        return false;

    MObject prtMpxData;
    plug.getValue( prtMpxData );
    MFnPluginData fnData( prtMpxData );
    MPxData* streamMPxData = fnData.data( &stat );
    if( stat != MStatus::kSuccess || streamMPxData == NULL ||
        streamMPxData->typeId() != frantic::maya::MPxParticleStream::id )
        return false;

    return true;
}

} // namespace maya
} // namespace frantic
