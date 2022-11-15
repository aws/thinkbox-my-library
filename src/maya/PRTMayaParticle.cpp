// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/PRTMayaParticle.hpp>
#include <frantic/maya/attributes.hpp>

#include <boost/shared_ptr.hpp>

#include <frantic/channels/channel_map.hpp>
#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/convert.hpp>
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>
#include <frantic/maya/util.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/shared_particle_container_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MStatus.h>

namespace frantic {
namespace maya {

namespace {

inline frantic::particles::streams::particle_istream_ptr getEmptyStream() {
    frantic::channels::channel_map lsChannelMap;
    lsChannelMap.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    lsChannelMap.end_channel_definition();

    return frantic::particles::streams::particle_istream_ptr(
        new frantic::particles::streams::empty_particle_istream( lsChannelMap ) );
}

} // namespace

const MTypeId PRTMayaParticle::typeId( 0x0011748f );
const MString PRTMayaParticle::typeName = "PRTMayaParticle";
const MString PRTMayaParticle::inParticleAttribute = "count";

MObject PRTMayaParticle::inConnect;
MObject PRTMayaParticle::outParticleStream;

PRTMayaParticle::PRTMayaParticle() {}

PRTMayaParticle::~PRTMayaParticle() {}

void* PRTMayaParticle::creator() { return new PRTMayaParticle; }

MStatus PRTMayaParticle::initialize() {
    using namespace std; // For CHECK_MSTATUS_AND_RETURN_IT
    MStatus status;

    // Particle Connection
    {
        MObject defaultArray;
        // MFnTypedAttribute fnTypedAttribute;
        // inConnect = fnTypedAttribute.create( "inConnect", "inConnect", MFnData::kVectorArray, defaultArray);
        // status = addAttribute( inConnect );
        // fnTypedAttribute.setHidden( true );

        MFnNumericAttribute fnNumericAttribute;
        inConnect = fnNumericAttribute.create( "inConnect", "inConnect", MFnNumericData::kInt );
        status = addAttribute( inConnect );
        fnNumericAttribute.setHidden( true );

        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // Particle Stream Output
    {
        MFnTypedAttribute fnTypedAttribute;
        outParticleStream = fnTypedAttribute.create( "outParticleStream", "outParticleStream",
                                                     frantic::maya::MPxParticleStream::id, MObject::kNullObj );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        status = addAttribute( outParticleStream );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // attributeAffects( inParticleStreamName, outParticleStream );

    return MS::kSuccess;
}

void PRTMayaParticle::postConstructor() {
    MStatus stat;

    // Output Particles
    {
        // prepare the default data
        MFnPluginData fnData;
        MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error( ( "DEBUG: maya_magma_mpxnode: fnData.create()" + stat.errorString() ).asChar() );
        frantic::maya::MPxParticleStream* mpxData = frantic::maya::mpx_cast<MPxParticleStream*>( fnData.data( &stat ) );
        if( !mpxData || stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: maya_magma_mpxnode: dynamic_cast<frantic::maya::MPxParticleStream*>(...) " +
                  stat.errorString() )
                    .asChar() );
        mpxData->setParticleSource( this );

        // get plug
        MObject obj = thisMObject();
        MFnDependencyNode depNode( obj, &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: maya_magma_mpxnode: could not get dependencyNode from thisMObject():" + stat.errorString() )
                    .asChar() );

        MPlug plug = depNode.findPlug( "outParticleStream", &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: maya_magma_mpxnode: could not find plug 'outParticleStream' from depNode: " +
                  stat.errorString() )
                    .asChar() );

        // set the default data on the plug
        FF_LOG( debug ) << "maya_magma_mpxnode::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }
}

MStatus PRTMayaParticle::compute( const MPlug& plug, MDataBlock& block ) {
    MStatus status = MPxNode::compute( plug, block );
    try {
        if( plug == outParticleStream ) {
            MDataHandle outputData = block.outputValue( outParticleStream );

            MFnPluginData fnData;
            MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &status );
            if( status != MStatus::kSuccess )
                return status;
            frantic::maya::MPxParticleStream* mpxData =
                frantic::maya::mpx_cast<MPxParticleStream*>( fnData.data( &status ) );
            if( mpxData == NULL )
                return MS::kFailure;
            mpxData->setParticleSource( this );

            outputData.set( mpxData );
            status = MS::kSuccess;
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MS::kFailure;
    }

    return status;
}

frantic::particles::streams::particle_istream_ptr
PRTMayaParticle::getParticleStream( const frantic::graphics::transform4f& objectSpace, const MDGContext& context,
                                    bool isViewport ) const {
    MStatus stat;

    // Get the input particle stream
    MObject particleStream = getConnectedMayaParticleStream( &stat );
    if( stat != MS::kSuccess ) {
        FF_LOG( debug )
            << ( ( "DEBUG: PRTMayaParticle: unable to get connected particle stream: " + stat.errorString() ).asChar() )
            << std::endl;
        return getEmptyStream();
    }
    MFnParticleSystem particleNode( particleStream, &stat );
    if( stat != MS::kSuccess ) {
        FF_LOG( debug )
            << ( ( "DEBUG: PRTMayaParticle: unable to get connected particle stream: " + stat.errorString() ).asChar() )
            << std::endl;
        return getEmptyStream();
    }

    // Ignore if not visible
    // bool visible =
    //	frantic::maya::get_boolean_attribute( particleNode , "visibility", context ) &&
    //	frantic::maya::get_boolean_attribute( particleNode, "primaryVisibility", context ) &&
    //	MRenderUtil::inCurrentRenderLayer( particleNode.dagPath() );
    // if ( !visible ) return getEmptyStream();

    boost::shared_ptr<frantic::particles::particle_array> particleArray( new frantic::particles::particle_array );

    // Set up channels
    frantic::channels::channel_map channels;
    channels.define_channel( frantic::maya::particles::PRTPositionChannelName, 3,
                             frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTVelocityChannelName, 3,
                             frantic::channels::data_type_float16 );
    channels.define_channel( frantic::maya::particles::PRTColorChannelName, 3, frantic::channels::data_type_float16 );
    channels.define_channel( frantic::maya::particles::PRTDensityChannelName, 1, frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTParticleIdChannelName, 1,
                             frantic::channels::data_type_int64 );
    channels.define_channel( frantic::maya::particles::PRTNormalChannelName, 3, frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTRotationChannelName, 3,
                             frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTTangentChannelName, 3, frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTEmissionChannelName, 3,
                             frantic::channels::data_type_float16 );
    channels.define_channel( frantic::maya::particles::PRTAbsorptionChannelName, 3,
                             frantic::channels::data_type_float16 );
    channels.define_channel( frantic::maya::particles::PRTAgeChannelName, 1, frantic::channels::data_type_float32 );
    channels.define_channel( frantic::maya::particles::PRTLifeSpanChannelName, 1,
                             frantic::channels::data_type_float32 );
    channels.end_channel_definition();

    bool ok = frantic::maya::particles::grab_maya_particles( particleNode, context, channels, *particleArray );
    if( !ok ) {
        FF_LOG( debug ) << ( ( "DEBUG: PRTMayaParticle: Unable to convert '" + particleNode.name() +
                               "' to PRT Particles: " + stat.errorString() )
                                 .asChar() )
                        << std::endl;
        return getEmptyStream();
    }

    // Transform code transferred from maya_ksr
    // Unfortunately, it seems that the only way to retrieve particles from maya is in world space.  However, in order
    // to apply motion blur, we need the particles to be in object space Note that if motion blur is disabled, we're
    // technically doing this un-transform only to re-transform it again, which is inefficient, and can introduce
    // numeric issues.
    // TODO: I'm leaving it like this for now only to keep the code simple as I develop.  Eventually, when this has
    // stabilized, this should go into the 'enableMotionBlur' condition in the if below and then the
    // non-motion-blur-case will just pass an identity transform.
    std::map<frantic::tstring, frantic::particles::prt::channel_interpretation::option> channelInterpretations;
    frantic::graphics::transform4f baseObjectSpace;
    MDagPath particleNodePath;
    stat = particleNode.getPath( particleNodePath );
    if( stat == MS::kSuccess ) {
        // We need to use the original particle object's transform to support instancing
        ok = maya_util::get_object_world_matrix( particleNodePath, context, baseObjectSpace );
    } else {
        ok = false;
    }
    if( !ok ) {
        baseObjectSpace = objectSpace;
        FF_LOG( debug )
            << ( ( "DEBUG: PRTMayaParticle: Unable to get base transform for '" + particleNode.name() + "'" ).asChar() )
            << std::endl;
    }
    frantic::particles::streams::transform_impl<float> transformer(
        baseObjectSpace.to_inverse(), frantic::graphics::transform4f::zero(), particleArray->get_channel_map(),
        channelInterpretations );
    for( frantic::particles::particle_array::iterator it = particleArray->begin(); it != particleArray->end(); ++it ) {
        transformer( *it );
    }

    // Done
    frantic::maya::PRTObjectBase::particle_istream_ptr outStream = frantic::maya::PRTObjectBase::particle_istream_ptr(
        new frantic::particles::streams::shared_particle_container_particle_istream<frantic::particles::particle_array>(
            particleArray ) );
    return outStream;
}

MObject PRTMayaParticle::getConnectedMayaParticleStream( MStatus* status ) const {
    MStatus stat;
    MObject obj = thisMObject();

    // Get the node
    MFnDependencyNode depNode( obj, &stat );
    if( stat != MStatus::kSuccess ) {
        if( status != NULL )
            ( *status ) = stat;
        return MObject::kNullObj;
    }

    // Get the attribute
    MPlug plug = depNode.findPlug( inConnect, &stat );
    if( stat != MStatus::kSuccess ) {
        if( status != NULL )
            ( *status ) = stat;
        return MObject::kNullObj;
    }

    // Get the connected node
    MPlugArray plugs;
    plug.connectedTo( plugs, true, false );
    unsigned int i;
    for( i = 0; i < plugs.length(); i++ ) {
        MObject currentObject = plugs[i].node( &stat );
        if( stat != MStatus::kSuccess )
            continue;

        MFnParticleSystem checkStream( currentObject, &stat );
        if( stat == MStatus::kSuccess )
            break;
    }

    if( status != NULL )
        ( *status ) = stat;
    if( i < plugs.length() )
        return plugs[i].node();
    return MObject::kNullObj;
}

/*
bool PRTMayaParticle::mayaParticleStreamHasDeformed( const MFnParticleSystem& particleStream, MStatus* status )
{
        MStatus stat;
        bool result = false;

        if ( !particleStream.isDeformedParticleShape( &stat ) ) {
                MObject deformedParticleShape = particleStream.deformedParticleShape( &stat );
                if (deformedParticleShape != MObject::kNullObj) {
                        result = true;
                }
        }

        if ( status != NULL ) (*status) = stat;
        return result;
}

bool PRTMayaParticle::mayaParticleStreamHasOriginal( const MFnParticleSystem& particleStream, MStatus* status )
{
        MStatus stat;
        bool result = false;

        if ( particleStream.isDeformedParticleShape( &stat ) ) {
                MObject deformedParticleShape = particleStream.originalParticleShape( &stat );
                if (deformedParticleShape != MObject::kNullObj) {
                        result = true;
                }
        }

        if ( status != NULL ) (*status) = stat;
        return result;
}
*/

MObject PRTMayaParticle::getPRTMayaParticleFromMayaParticleStream( const MFnParticleSystem& particleStream,
                                                                   MStatus* status ) {
    MStatus stat;

    // Get the attribute
    MPlug plug = particleStream.findPlug( inParticleAttribute, &stat );
    if( stat != MStatus::kSuccess ) {
        if( status != NULL )
            ( *status ) = stat;
        return MObject::kNullObj;
    }

    // Get the connected node
    MPlugArray plugs;
    plug.connectedTo( plugs, false, true );
    unsigned int i;
    for( i = 0; i < plugs.length(); i++ ) {
        MObject currentObject = plugs[i].node( &stat );
        if( stat != MStatus::kSuccess )
            continue;

        MFnDependencyNode checkStream( currentObject, &stat );
        if( stat != MStatus::kSuccess )
            continue;

        MTypeId id = checkStream.typeId( &stat );
        if( stat != MStatus::kSuccess )
            continue;
        if( id == PRTMayaParticle::typeId )
            break;
    }

    if( i < plugs.length() ) {
        if( status != NULL )
            ( *status ) = stat;
        return plugs[i].node();
    }

    if( status != NULL )
        ( *status ) = stat;
    return MObject::kNullObj;
}

MObject PRTMayaParticle::getPRTMayaParticleFromMayaParticleStreamCheckDeformed( const MFnParticleSystem& particleStream,
                                                                                MStatus* status, bool autoCreate ) {
    MStatus stat;

    // Get to the deformed version.  We're ignoring the original always
    if( !particleStream.isDeformedParticleShape( &stat ) ) {
        MObject deformedParticleShape = particleStream.deformedParticleShape( &stat );
        if( stat && deformedParticleShape != MObject::kNullObj ) {
            MFnParticleSystem deformedParticleStream( deformedParticleShape, &stat );
            if( stat == MS::kSuccess ) {
                frantic::tstring originalParticlesName = frantic::maya::from_maya_t( particleStream.particleName() );
                frantic::tstring deformedParticlesName =
                    frantic::maya::from_maya_t( deformedParticleStream.particleName() );
                if( originalParticlesName != deformedParticlesName ) {
                    return getPRTMayaParticleFromMayaParticleStreamCheckDeformed( deformedParticleStream, status,
                                                                                  autoCreate );
                }
            }
        }
    }
    // From this point, we're always looking at the deformed version if it exists

    // Check if we have something
    MObject result = getPRTMayaParticleFromMayaParticleStream( particleStream, &stat );
    if( result != MObject::kNullObj && stat == MS::kSuccess ) {
        // We found the wrapper.  We're done
        if( status != NULL )
            ( *status ) = stat;
        return result;
    }

    // We found nothing.  If the original had the wrapper, we need to update it to attach to the deformed version
    if( particleStream.isDeformedParticleShape( &stat ) ) {
        MObject originalParticleShape = particleStream.originalParticleShape( &stat );
        if( originalParticleShape != MObject::kNullObj ) {
            MFnParticleSystem originalParticleStream( originalParticleShape );
            MObject originalWrapper = getPRTMayaParticleFromMayaParticleStream( originalParticleStream, &stat );

            if( originalWrapper != MObject::kNullObj && stat == MS::kSuccess ) {
                // The original particle stream had the wrapper.  Reconnect and try again.
                MFnDependencyNode wrapper( originalWrapper );
                MString originalParticleName = maya_util::get_node_full_name( originalParticleStream );
                MString deformedParticleName = maya_util::get_node_full_name( particleStream );
                MString wrapperParticleName = maya_util::get_node_full_name( wrapper );

                stat = MGlobal::executeCommand(
                    "string $original = \"" + originalParticleName + "\";\n" + "string $deformed = \"" +
                    deformedParticleName + "\";\n" + "string $wrapper = \"" + wrapperParticleName + "\";\n" +
                    "disconnectAttr ( $original + \"." + PRTMayaParticle::inParticleAttribute +
                    "\" ) ( $wrapper + \".inConnect\" );\n" + "connectAttr ( $deformed + \"." +
                    PRTMayaParticle::inParticleAttribute + "\" ) ( $wrapper + \".inConnect\" );\n" );
                if( stat == MS::kSuccess ) {
                    return getPRTMayaParticleFromMayaParticleStreamCheckDeformed( particleStream, status, autoCreate );
                }

                // Error running the command
                if( status != NULL )
                    ( *status ) = stat;
                return MObject::kNullObj;
            }
        }
    }

    // Passive colliders are treated like particles/nParticles in maya, and therefore generate particles at the mesh
    // vertices. We don't like this behaviour so we skip the nRigid objects. This behaviour is duplicated in the script
    // version.

    // If auto create request, create the node and try again
    if( autoCreate ) {
        if( particleStream.typeName() != "nRigid" ) {
            MString particleName = maya_util::get_node_full_name( particleStream );
            stat = MGlobal::executeCommand( "string $prtwrap = `createNode \"" + PRTMayaParticle::typeName +
                                            "\" -ss`;\n" + "string $mayapart = \"" + particleName + "\";\n" +
                                            "connectAttr ( $mayapart + \"." + PRTMayaParticle::inParticleAttribute +
                                            "\" ) ( $prtwrap + \".inConnect\" );\n" );
            if( stat == MS::kSuccess ) {
                return getPRTMayaParticleFromMayaParticleStreamCheckDeformed( particleStream, status, false );
            }
        }
    }

    if( status != NULL )
        ( *status ) = stat;
    return MObject::kNullObj;
}

} // namespace maya
} // namespace frantic
