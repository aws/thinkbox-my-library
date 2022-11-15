// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/channels/channel_map.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/strings/tstring.hpp>
#include <maya/MDGContext.h>
#include <maya/MFnParticleSystem.h>

namespace frantic {
namespace maya {
namespace particles {

extern const frantic::tstring MayaPositionChannelName;
extern const frantic::tstring MayaVelocityChannelName;
extern const frantic::tstring MayaParticleIdChannelName;
extern const frantic::tstring MayaDensityChannelName;
extern const frantic::tstring MayaColorChannelName;
extern const frantic::tstring MayaNormalChannelName;
extern const frantic::tstring MayaIncandescenceChannelName;
extern const frantic::tstring MayaEmissionChannelName;
extern const frantic::tstring MayaGlobalRedChannelName;
extern const frantic::tstring MayaGlobalGreenChannelName;
extern const frantic::tstring MayaGlobalBlueChannelName;
extern const frantic::tstring MayaRotationChannelName;
extern const frantic::tstring MayaAgeChannelName;
extern const frantic::tstring MayaLifeSpanChannelName;

extern const frantic::tstring PRTPositionChannelName;
extern const frantic::tstring PRTVelocityChannelName;
extern const frantic::tstring PRTParticleIdChannelName;
extern const frantic::tstring PRTDensityChannelName;
extern const frantic::tstring PRTNormalChannelName;
extern const frantic::tstring PRTColorChannelName;
extern const frantic::tstring PRTEmissionChannelName;
extern const frantic::tstring PRTTangentChannelName;
extern const frantic::tstring PRTAbsorptionChannelName;
extern const frantic::tstring PRTRotationChannelName;
extern const frantic::tstring PRTAgeChannelName;
extern const frantic::tstring PRTLifeSpanChannelName;

bool get_prt_channel_name( const frantic::tstring& mayaName, frantic::tstring& prtName );
void get_prt_channel_name_default( const frantic::tstring& mayaName, frantic::tstring& resultName );
bool get_maya_channel_name( const frantic::tstring& prtName, frantic::tstring& mayaName );
void get_maya_channel_name_default( const frantic::tstring& prtName, frantic::tstring& resultName );

bool grab_maya_particles( const MFnParticleSystem& particleSystem, const MDGContext& currentContext,
                          const frantic::channels::channel_map& channelMap,
                          frantic::particles::particle_array& outParticleArray );

} // namespace particles
} // namespace maya
} // namespace frantic
