// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/particles/particles.hpp>

#include <maya/MDGContext.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MVectorArray.h>

#include <frantic/channels/named_channel_data.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/maya/convert.hpp>

#include <boost/bimap.hpp>
#include <vector>

using namespace frantic::channels;
using namespace frantic::particles;
using namespace frantic::graphics;

using std::cerr;
using std::endl;

namespace {

// the 'left->right' set is PRT->Maya, the 'right->left' set is Maya->PRT
typedef boost::bimap<frantic::tstring, frantic::tstring> prt_maya_bimap_t;
prt_maya_bimap_t PRTMayaBiMap;

void enusure_bimap_initialized() {
    static bool initialized = false;

    if( !initialized ) {
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTPositionChannelName,
                                                           frantic::maya::particles::MayaPositionChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTVelocityChannelName,
                                                           frantic::maya::particles::MayaVelocityChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTParticleIdChannelName,
                                                           frantic::maya::particles::MayaParticleIdChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTDensityChannelName,
                                                           frantic::maya::particles::MayaDensityChannelName ) );
        PRTMayaBiMap.insert(
            prt_maya_bimap_t::value_type( frantic::maya::particles::PRTNormalChannelName,
                                          frantic::maya::particles::MayaNormalChannelName ) ); // TODO: see below
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTRotationChannelName,
                                                           frantic::maya::particles::MayaRotationChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTColorChannelName,
                                                           frantic::maya::particles::MayaColorChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTEmissionChannelName,
                                                           frantic::maya::particles::MayaEmissionChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTAgeChannelName,
                                                           frantic::maya::particles::MayaAgeChannelName ) );
        PRTMayaBiMap.insert( prt_maya_bimap_t::value_type( frantic::maya::particles::PRTLifeSpanChannelName,
                                                           frantic::maya::particles::MayaLifeSpanChannelName ) );
        initialized = true;
    }
}

typedef std::pair<data_type_t, std::size_t> channel_type;

bool is_vector_channel_type( channel_type type ) {
    return is_channel_data_type_float( type.first ) && type.second == 3;
}

bool is_float_channel_type( channel_type type ) { return is_channel_data_type_float( type.first ) && type.second == 1; }

bool is_int_channel_type( channel_type type ) { return is_channel_data_type_signed( type.first ); }

void report_length_error( const frantic::tstring& channelName, size_t actualLength, size_t expectedLength ) {
    std::ostringstream errorText;
    errorText << "Particle channel \"" << frantic::strings::to_string( channelName ) << "\" has size " << actualLength
              << ", differing from the total number of particles, " << expectedLength << ".";
    MGlobal::displayError( errorText.str().c_str() );
}

MStatus get_attribute_value( const MFnDependencyNode& fnNode, const frantic::tstring& attributeName,
                             const MDGContext& currentContext, MObject& outValue ) {
    MStatus status;

    MPlug plug = fnNode.findPlug( attributeName.c_str(), true, &status );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    // copy currentContext because MPlug.getValue() takes it as a non-const reference
    MDGContext currentContextCopy( currentContext );

    status = plug.getValue( outValue, currentContextCopy );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    return status;
}

MStatus copy_value( const MObject& obj, MVectorArray& out ) {
    MStatus status;

    MFnVectorArrayData fnVectorArray( obj, &status );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    status = fnVectorArray.copyTo( out );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    return status;
}

MStatus copy_position( const MFnParticleSystem& particleSystem, const MDGContext& currentContext, MVectorArray& out ) {
    MStatus stat;

    // If the particles are cached using nCache, then the Positions we retrieve
    // using MFnParticleSystem are incorrect (all zero).
    // Here we attempt to get the positions from the shape's "worldPosition"
    // attribute, which seems to be correct when using an nCache.
    MObject value;
    stat = get_attribute_value( particleSystem, _T( "worldPosition" ), currentContext, value );
    if( stat ) {
        stat = copy_value( value, out );
        CHECK_MSTATUS_AND_RETURN_IT( stat );
    } else {
        particleSystem.position( out );
    }

    return MStatus::kSuccess;
}

} // namespace

namespace frantic {
namespace maya {
namespace particles {

const frantic::tstring MayaPositionChannelName = _T( "position" );
const frantic::tstring MayaVelocityChannelName = _T( "velocity" );
const frantic::tstring MayaParticleIdChannelName = _T( "particleId" );
const frantic::tstring MayaDensityChannelName = _T( "opacity" );
const frantic::tstring MayaColorChannelName = _T( "rgb" );
// TODO: find the proper normal channel to use.  normalDir is just an int
// http://download.autodesk.com/us/maya/2009help/index.html?url=Particles_Use_lights_reflections_refractions_and_shadows.htm,topicNumber=d0e383417
// http://www.creativecrash.com/forums/dynamics-amp-effects/topics/streak-multi-streak-normaldir
const frantic::tstring MayaNormalChannelName = _T( "normalDir" );
const frantic::tstring MayaRotationChannelName = _T( "rotation" );
const frantic::tstring MayaEmissionChannelName = _T( "incandescence" );
const frantic::tstring MayaAgeChannelName = _T( "age" );
const frantic::tstring MayaLifeSpanChannelName = _T( "lifespan" );

const frantic::tstring MayaGlobalRedChannelName = _T( "colorRed" );
const frantic::tstring MayaGlobalGreenChannelName = _T( "colorGreen" );
const frantic::tstring MayaGlobalBlueChannelName = _T( "colorBlue" );

const frantic::tstring PRTPositionChannelName = _T( "Position" );
const frantic::tstring PRTVelocityChannelName = _T( "Velocity" );
const frantic::tstring PRTParticleIdChannelName = _T( "ID" );
const frantic::tstring PRTDensityChannelName = _T( "Density" );
const frantic::tstring PRTNormalChannelName = _T( "Normal" );
const frantic::tstring PRTRotationChannelName = _T( "Rotation" );
const frantic::tstring PRTColorChannelName = _T( "Color" );
const frantic::tstring PRTEmissionChannelName = _T( "Emission" );
const frantic::tstring PRTTangentChannelName = _T( "Tangent" );
const frantic::tstring PRTAbsorptionChannelName = _T( "Absorption" );
const frantic::tstring PRTAgeChannelName = _T( "Age" );
const frantic::tstring PRTLifeSpanChannelName = _T( "LifeSpan" );

/**
 * Get the standard PRT channel name, given a maya channel name. If no such
 * mapping exists, prtName is not initialized and the method returns false.
 *
 * @param  mayaName the name of the channel in maya particle systems
 * @param  outPRTName output location of the PRT channel name, if found
 * @return true if a mapping was found, false otherwise
 */
bool get_prt_channel_name( const frantic::tstring& mayaName, frantic::tstring& outPRTName ) {
    enusure_bimap_initialized();
    prt_maya_bimap_t::right_const_iterator it = PRTMayaBiMap.right.find( mayaName );

    if( it == PRTMayaBiMap.right.end() )
        return false;

    outPRTName = it->second;
    return true;
}

/**
 * Same as get_prt_channel_name, but in the event no mapping is found,
 * resultName will simply be set to mayaName
 *
 * @param  mayaName the name of the channel in maya particle systems
 * @param  resultName output location of the channel name
 */
void get_prt_channel_name_default( const frantic::tstring& mayaName, frantic::tstring& resultName ) {
    if( !get_prt_channel_name( mayaName, resultName ) ) {
        resultName = mayaName;
    }
}

/**
 * Get the standard Maya particle system channel name, given a maya channel
 * name. If no such mapping exists, prtName is not initialized and the method
 * returns false.
 *
 * @param  prtName the name of the channel in PRT files
 * @param  outMayaName output location of the Maya channel name, if found
 * @return true if a mapping was found, false otherwise
 */
bool get_maya_channel_name( const frantic::tstring& prtName, frantic::tstring& outMayaName ) {
    enusure_bimap_initialized();
    prt_maya_bimap_t::left_const_iterator it = PRTMayaBiMap.left.find( prtName );

    if( it == PRTMayaBiMap.left.end() )
        return false;

    outMayaName = it->second;
    return true;
}

/**
 * Same as get_prt_channel_name, but in the event no mapping is found,
 * resultName will simply be set to prtName
 *
 * @param  prtName the name of the channel in PRT files
 * @param  resultName output location of the channel name
 */
void get_maya_channel_name_default( const frantic::tstring& prtName, frantic::tstring& resultName ) {
    if( !get_maya_channel_name( prtName, resultName ) ) {
        resultName = prtName;
    }
}

/**
 * Retrieves the channels specified in the channelMap object from the given particleSystem at the specified time.
 * The channels should be specified using their krakatoa name, not the maya channel name (this method will perform
 * the appropriate conversions).  It will also perform a resonably intelligent name-resolution scheme, where particle
 * channels with a 'PP' (i.e. Per-Particle) suffix will be searched first.  It will also search for both per-particle
 * and per-object attributes, though all information will always be copied out per-particle. Note that the particles
 * will be retrieved in world-space, not object space, since this is the only way maya will expose its particle data.
 * You'll have to manually reverse the transform if you want object space particles.
 *
 * @param particleSystem particle system object to retrieve particles from
 * @param currentContext scene time at which to retrieve particle data (this actually has no effect on the output, maya
 * will always just pass back the particles at the current scene time, hence this should be removed eventually)
 * @param channelMap specifies which channels of particle data to retrieve
 * @param outParticleArray result where the retrieved particles will be stored
 * @return true if the procedure was successful, false if there was an error
 */
bool grab_maya_particles( const MFnParticleSystem& particleSystem, const MDGContext& currentContext,
                          const channel_map& channelMap, particle_array& outParticleArray ) {
    outParticleArray.clear();
    outParticleArray.set_channel_map( channelMap );
    outParticleArray.resize( particleSystem.count() );

    // cycle through all of the selected channels and copy out all requested information for each particle
    for( size_t i = 0; i < channelMap.channel_count(); ++i ) {
        const channel& currentChannel = channelMap[i];
        frantic::tstring channelName = currentChannel.name();
        frantic::tstring mayaName;
        get_maya_channel_name_default( channelName, mayaName );
        channel_type currentType = std::make_pair( currentChannel.data_type(), currentChannel.arity() );
        MObject targetPerParticleArray;
        MObject targetParticleArray;
        MObject selectedArray = MObject::kNullObj;
        // search both for a 'Per Particle (PP)' variant of the specified channel, and the raw channel name
        particleSystem.findPlug( ( mayaName + _T( "PP" ) ).c_str() )
            .getValue( targetPerParticleArray, const_cast<MDGContext&>( currentContext ) );
        particleSystem.findPlug( mayaName.c_str() )
            .getValue( targetParticleArray, const_cast<MDGContext&>( currentContext ) );

        if( is_vector_channel_type( currentType ) ) {
            MVectorArray vectorArray;
            channel_cvt_accessor<vector3f> vectorAccessor = channelMap.get_cvt_accessor<vector3f>( channelName );

            // First check for default-defined maya particle channel names (we always expect these to be defined or have
            // reasonable default values)
            bool channelFound = true;
            if( channelName == PRTPositionChannelName ) {
#if MAYA_API_VERSION >= 202200
                particleSystem.position( vectorArray );
#else
                MStatus stat;
                stat = copy_position( particleSystem, currentContext, vectorArray );
                if( !stat ) {
                    MGlobal::displayError( "Unable to get position from particle system" );
                    return false;
                }
#endif
            } else if( channelName == PRTColorChannelName ) {
                particleSystem.rgb( vectorArray );
            } else if( channelName == PRTVelocityChannelName ) {
                particleSystem.velocity( vectorArray );
            } else if( targetPerParticleArray.apiType() == MFn::kVectorArrayData ) {
                MFnVectorArrayData arrayVectorObject( targetPerParticleArray );
                arrayVectorObject.copyTo( vectorArray );
            } else if( targetParticleArray.apiType() == MFn::kVectorArrayData ) {
                MFnVectorArrayData arrayVectorObject( targetParticleArray );
                arrayVectorObject.copyTo( vectorArray );
            } else {
                channelFound = false;
            }

            if( channelFound ) {
                if( vectorArray.length() < outParticleArray.size() ) {
                    report_length_error( mayaName, vectorArray.length(), outParticleArray.size() );
                    return false;
                }
                unsigned int currentParticle = 0;
                for( particle_array::iterator it = outParticleArray.begin(); it != outParticleArray.end(); ++it ) {
                    vector3f vectorValue( (float)vectorArray[currentParticle].x, (float)vectorArray[currentParticle].y,
                                          (float)vectorArray[currentParticle].z );
                    vectorAccessor.set( *it, vectorValue );
                    ++currentParticle;
                }
            } else {
                frantic::tstring systemName = frantic::maya::from_maya_t( particleSystem.particleName() );
                FF_LOG( debug ) << _T( "Neither \"" ) + mayaName + _T( "\" or \"" ) + mayaName +
                                       _T( "PP\" channels were found in the maya particle system \"" ) + systemName +
                                       _T( "\". The \"" ) + channelName + _T( "\" channel will default to [0,0,0]\n" );
                // channel not found (often happens for normalDir), set the channel to all zeros.
                vector3f defaultValue( 0.0f, 0.0f, 0.0f );
                for( particle_array::iterator it = outParticleArray.begin(); it != outParticleArray.end(); ++it ) {
                    vectorAccessor.set( *it, defaultValue );
                }
            }
        } else if( is_float_channel_type( currentType ) ) {
            MDoubleArray doubleArray;
            channel_cvt_accessor<double> doubleAccessor = channelMap.get_cvt_accessor<double>( channelName );

            if( channelName == PRTDensityChannelName ) {
                particleSystem.opacity( doubleArray );
            } else if( channelName == PRTAgeChannelName ) {
                particleSystem.age( doubleArray );
            } else if( channelName == PRTLifeSpanChannelName ) {
                particleSystem.lifespan( doubleArray );
            } else if( targetPerParticleArray.apiType() == MFn::kDoubleArrayData ) {
                MFnDoubleArrayData arrayDoubleObject( targetPerParticleArray );
                arrayDoubleObject.copyTo( doubleArray );
            } else if( targetParticleArray.apiType() == MFn::kDoubleArrayData ) {
                MFnDoubleArrayData arrayDoubleObject( targetPerParticleArray );
                arrayDoubleObject.copyTo( doubleArray );
            } else {
                MStatus getStatus;
                double value = particleSystem.findPlug( mayaName.c_str() )
                                   .asDouble( const_cast<MDGContext&>( currentContext ), &getStatus );

                if( getStatus == MStatus::kSuccess ) {
                    doubleArray.setLength( (unsigned int)outParticleArray.size() );

                    for( unsigned int i = 0; i < outParticleArray.size(); ++i ) {
                        doubleArray[i] = value;
                    }
                } else {
                    // instead of erroring, maybe we should just set it to zero (that is what the "vector" type is
                    // doing, since KMY requests normalDir, and it's not usually there)
                    std::ostringstream errorText;
                    errorText << "Could not get \"" << frantic::strings::to_string( mayaName )
                              << "\" from NParticle object.";
                    MGlobal::displayError( errorText.str().c_str() );
                    return false;
                }
            }

            if( doubleArray.length() < outParticleArray.size() ) {
                report_length_error( mayaName, doubleArray.length(), outParticleArray.size() );
                return false;
            }

            unsigned int currentParticle = 0;

            for( particle_array::iterator it = outParticleArray.begin(); it != outParticleArray.end(); ++it ) {
                double doubleValue = doubleArray[currentParticle];
                doubleAccessor.set( *it, doubleValue );
                ++currentParticle;
            }
        } else if( is_int_channel_type( currentType ) ) {
            std::vector<boost::int64_t> intArray( outParticleArray.size() );
            channel_cvt_accessor<boost::int64_t> intAccessor =
                channelMap.get_cvt_accessor<boost::int64_t>( channelName );

            // Maya does not allow specifying integers as per-particle data, so they will always be found as floats
            // (even particleId)
            if( targetPerParticleArray.apiType() == MFn::kDoubleArrayData ) {
                selectedArray = targetPerParticleArray;
            } else if( targetParticleArray.apiType() == MFn::kDoubleArrayData ) {
                selectedArray = targetParticleArray;
            }

            if( selectedArray.apiType() != MFn::kInvalid ) {
                MFnDoubleArrayData doubleArrayObject( selectedArray );

                if( doubleArrayObject.length() < outParticleArray.size() ) {
                    if( doubleArrayObject.length() == 0 && channelName == _T( "ID" ) ) {
                        for( unsigned int i = 0; i < outParticleArray.size(); ++i ) {
                            intArray[i] = static_cast<boost::int64_t>( i );
                        }
                    } else {
                        report_length_error( mayaName, doubleArrayObject.length(), outParticleArray.size() );
                        return false;
                    }
                } else {
                    for( unsigned int i = 0; i < outParticleArray.size(); ++i ) {
                        intArray[i] = (boost::int64_t)doubleArrayObject[i];
                    }
                }

            } else {
                MStatus getStatus;
                boost::int64_t value = (boost::int64_t)particleSystem.findPlug( mayaName.c_str() )
                                           .asInt( const_cast<MDGContext&>( currentContext ), &getStatus );

                if( getStatus == MStatus::kSuccess ) {
                    for( unsigned int i = 0; i < outParticleArray.size(); ++i ) {
                        intArray[i] = value;
                    }
                } else {
                    // instead of erroring, maybe we should just set it to zero (that is what the "vector" type is
                    // doing, since KMY requests normalDir, and it's not usually there)
                    std::ostringstream errorText;
                    errorText << "Could not get \"" << frantic::strings::to_string( mayaName )
                              << "\" from NParticle object.";
                    MGlobal::displayError( errorText.str().c_str() );
                    return false;
                }
            }

            size_t currentParticle = 0;

            for( particle_array::iterator it = outParticleArray.begin(); it != outParticleArray.end(); ++it ) {
                boost::int64_t intValue = intArray[currentParticle];
                intAccessor.set( *it, intValue );
                ++currentParticle;
            }
        }
    }

    return true;
}

} // namespace particles
} // namespace maya
} // namespace frantic
