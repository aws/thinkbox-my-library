// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/particles/texture_evaluation_particle_istream.hpp>

#include <frantic/channels/channel_map_adaptor.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>
#include <limits>

#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MRenderUtil.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

namespace frantic {
namespace maya {
namespace particles {

//
//
// functions that work on particle_arrays
//
//

maya_texture_type_t get_texture_type( const std::string& mayaMaterialNodeName ) {
    MSelectionList list;
    // create a custom selection list
    list.add( mayaMaterialNodeName.c_str() );
    MObject mayaMaterialObject;
    list.getDependNode( 0, mayaMaterialObject );
    MFnDependencyNode mayaMaterialDepNode( mayaMaterialObject );

    std::string classification = MFnDependencyNode::classification( mayaMaterialDepNode.typeName() ).asChar();
    FF_LOG( debug ) << "Node \"" << frantic::strings::to_tstring( mayaMaterialNodeName ) << "\" has classification \""
                    << frantic::strings::to_tstring( classification ) << "\".\n";
    if( frantic::strings::ends_with( frantic::strings::to_lower( classification ), "texture/2d" ) )
        return TEXTURE_TYPE_2D;
    if( frantic::strings::ends_with( frantic::strings::to_lower( classification ), "texture/3d" ) )
        return TEXTURE_TYPE_3D;
    return TEXTURE_TYPE_UNSUPPORTED;
}

void apply_2d_texture_evaluation( frantic::particles::particle_array& pArray, size_t numParticles,
                                  const std::string& mayaMaterialNodeName, const MFloatArray& uArray,
                                  const MFloatArray& vArray, const frantic::tstring& outputChannelName ) {
    FF_LOG( debug ) << "Calling apply_2d_texture_evaluation for array of " << numParticles << " particles.\n";

    MFloatVectorArray textureEvalColors;
    MFloatVectorArray textureEvalAlphas;

    MFloatMatrix cameraMatrix;
    cameraMatrix.setToIdentity();
    MRenderUtil::sampleShadingNetwork(
        MString( mayaMaterialNodeName.c_str() ) +
            ".outColor", // shading node name's output plug (always has an outColor. it also has an outAlpha which can
                         // be used, I think it's the grey-scale version of outColor. not sure.)
        (int)numParticles,                   // num samples
        false,                               // use shadow maps
        false,                               // reuse shadow maps
        cameraMatrix,                        // camera matrix
        NULL,                                // 3d uvw points
        const_cast<MFloatArray*>( &uArray ), // ucoords only used for 2d maps
        const_cast<MFloatArray*>( &vArray ), // vcoords only used for 2d maps
        NULL,                                // normals not required
        NULL,                                // 3d uvw "reference" points
        NULL,                                // tangent u's
        NULL,                                // tangent v's
        NULL,                                // filter sizes
        textureEvalColors,                   // output color array
        textureEvalAlphas // output transparencies. we aren't using this, so it's wasted memory. MAYBE The proper way to
                          // do it would be to use this as an alpha to blend with an existing color.
    );

    // set the final colors
    frantic::channels::data_type_t dataType;
    size_t arity;
    pArray.get_channel_map().get_channel_definition( outputChannelName, dataType, arity );
    if( arity == 3 ) {
        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> outAcc =
            pArray.get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( outputChannelName );
        for( unsigned int i = 0; i < numParticles; ++i )
            outAcc.set( pArray[i], frantic::graphics::vector3f( textureEvalColors[i].x, textureEvalColors[i].y,
                                                                textureEvalColors[i].z ) );
    } else { // arity == 1
        frantic::channels::channel_cvt_accessor<float> outAcc =
            pArray.get_channel_map().get_cvt_accessor<float>( outputChannelName );
        for( unsigned int i = 0; i < numParticles; ++i )
            outAcc.set( pArray[i],
                        ( textureEvalColors[i].x + textureEvalColors[i].y + textureEvalColors[i].z ) / 3.0f );
    }
}

void apply_3d_texture_evaluation( frantic::particles::particle_array& pArray, size_t numParticles,
                                  const std::string& mayaMaterialNodeName, const MFloatPointArray& uvwArray,
                                  const frantic::tstring& outputChannelName ) {
    FF_LOG( debug ) << "Calling apply_3d_texture_evaluation for array of " << numParticles << " particles.\n";

    MFloatVectorArray textureEvalColors;
    MFloatVectorArray textureEvalAlphas;

    MFloatMatrix cameraMatrix;
    cameraMatrix.setToIdentity();
    MRenderUtil::sampleShadingNetwork(
        MString( mayaMaterialNodeName.c_str() ) +
            ".outColor", // shading node name's output plug (always has an outColor. it also has an outAlpha which can
                         // be used, I think it's the grey-scale version of outColor. not sure.)
        (int)numParticles,                          // num samples
        false,                                      // use shadow maps
        false,                                      // reuse shadow maps
        cameraMatrix,                               // camera matrix
        const_cast<MFloatPointArray*>( &uvwArray ), // 3d uvw points
        NULL,                                       // ucoords only used for 2d maps
        NULL,                                       // vcoords only used for 2d maps
        NULL,                                       // normals not required
        const_cast<MFloatPointArray*>( &uvwArray ), // 3d uvw "reference" points
        NULL,                                       // tangent u's
        NULL,                                       // tangent v's
        NULL,                                       // filter sizes
        textureEvalColors,                          // output color array
        textureEvalAlphas // output transparencies. we aren't using this, so it's wasted memory. MAYBE The proper way to
                          // do it would be to use this as an alpha to blend with an existing color.
    );

    // set the final colors
    frantic::channels::data_type_t dataType;
    size_t arity;
    pArray.get_channel_map().get_channel_definition( outputChannelName, dataType, arity );
    if( arity == 3 ) {
        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> outAcc =
            pArray.get_channel_map().get_cvt_accessor<frantic::graphics::vector3f>( outputChannelName );
        for( unsigned int i = 0; i < numParticles; ++i )
            outAcc.set( pArray[i], frantic::graphics::vector3f( textureEvalColors[i].x, textureEvalColors[i].y,
                                                                textureEvalColors[i].z ) );
    } else { // arity == 1
        frantic::channels::channel_cvt_accessor<float> outAcc =
            pArray.get_channel_map().get_cvt_accessor<float>( outputChannelName );
        for( unsigned int i = 0; i < numParticles; ++i )
            outAcc.set( pArray[i],
                        ( textureEvalColors[i].x + textureEvalColors[i].y + textureEvalColors[i].z ) / 3.0f );
    }
}

//
//
// stream
//
//

texture_evaluation_particle_istream::texture_evaluation_particle_istream(
    boost::shared_ptr<frantic::particles::streams::particle_istream> pin, const std::string& mayaMaterialNodeName,
    const frantic::tstring& uvwChannelName, const frantic::tstring resultChannelName, size_t bufferSize )
    : m_delegate( pin )
    , m_uvwChannelName( uvwChannelName )
    , m_resultChannelName( resultChannelName )
    , m_mayaMaterialNodeName( mayaMaterialNodeName )
    , m_bufferedParticlesIndex( std::numeric_limits<size_t>::max() ) {
    FF_LOG( debug ) << "Adding texture evaulation of \"" << frantic::strings::to_tstring( mayaMaterialNodeName )
                    << "\" to stream named \"" << m_delegate->name() << "\".\n";

    if( !m_delegate->get_native_channel_map().has_channel( frantic::strings::to_tstring( m_uvwChannelName ) ) )
        throw std::runtime_error( "material_evaluation_particle_istream error: The specified UVW channel: \"" +
                                  frantic::strings::to_string( uvwChannelName ) +
                                  "\" does not appear in the delegate stream's native channel map." );

    m_currentBufferSize = 0;
    m_particleIndex = 0;
    m_bufferedParticlesIndex = 0;

    // determine buffer size (normally it's user-inputted)
    m_maxBufferSize = std::min( (boost::int64_t)bufferSize, m_delegate->particle_count() );
    if( m_maxBufferSize == -1 )
        m_maxBufferSize = bufferSize;

    // determine texture type, either 2d or 3d
    m_is2dTexture = false;
    maya_texture_type_t textureType = get_texture_type( mayaMaterialNodeName );
    if( textureType == TEXTURE_TYPE_2D )
        m_is2dTexture = true;
    if( textureType == TEXTURE_TYPE_UNSUPPORTED )
        throw std::runtime_error( "material_evaluation_particle_istream error: The specified material node: \"" +
                                  mayaMaterialNodeName +
                                  "\" does not appear to be a valid 2d or 3d texture map. Texture evaluation is only "
                                  "supported for 2d and 3d texture maps." );

    init_channel_map( m_delegate->get_channel_map() );
}

void texture_evaluation_particle_istream::set_channel_map( const frantic::channels::channel_map& particleChannelMap ) {
    if( m_particleIndex > 0 ) // this is an unfortunate consequence of pre-buffering particles.
        throw std::runtime_error( "texture_evaluation_particle_istream::set_channel_map can only be called prior to "
                                  "calling get_particle()." );
    init_channel_map( particleChannelMap );
}

void texture_evaluation_particle_istream::set_default_particle( char* rawParticleBuffer ) {
    memcpy( &m_defaultParticle[0], rawParticleBuffer, m_channelMap.structure_size() );
}

const frantic::channels::channel_map& texture_evaluation_particle_istream::get_channel_map() const {
    return m_channelMap;
}

const frantic::channels::channel_map& texture_evaluation_particle_istream::get_native_channel_map() const {
    return m_nativeChannelMap;
}

bool texture_evaluation_particle_istream::get_particle( char* outParticleBuffer ) {

    // fill the buffer if need be
    // the first time though, these will be zero, and the buffer will be filled.
    // the next times though, it will only re-fill when it gets to the end
    if( m_bufferedParticlesIndex == m_currentBufferSize ) {
        m_bufferedParticlesIndex = 0;
        if( m_is2dTexture )
            m_currentBufferSize = texturemap_2d_fill_particle_buffer();
        else
            m_currentBufferSize = texturemap_3d_fill_particle_buffer();
        if( m_currentBufferSize == 0 ) {
            m_bufferedParticles.clear(); // deallocate our internal buffer (we don't need the memory any more).
            return false;                // return if the delegate stream is exhaused
        }
    }

    // get the next particle
    memcpy( outParticleBuffer, m_bufferedParticles[m_bufferedParticlesIndex], m_channelMap.structure_size() );

    ++m_bufferedParticlesIndex;
    ++m_particleIndex;
    return true;
}

bool texture_evaluation_particle_istream::get_particles( char* buffer, std::size_t& numParticles ) {
    // This function could be optimized, instead of just taking one by one from the stream. We have a particle_array
    // after all.
    size_t offset = 0;
    size_t particleSize = m_channelMap.structure_size();
    while( offset < numParticles * particleSize ) {
        if( !get_particle( buffer + offset ) ) {
            numParticles = offset / particleSize;
            return false;
        }
        offset += particleSize;
    }
    return true;
}

void texture_evaluation_particle_istream::init_channel_map( const frantic::channels::channel_map& inputChannelMap ) {
    // if a default particle was previously set, copy over the old default structure, otherwise, create a new default
    // particle
    if( m_defaultParticle.size() > 0 ) {
        std::vector<char> newDefaultParticle( inputChannelMap.structure_size() );
        frantic::channels::channel_map_adaptor oldToNewChannelMapAdaptor( inputChannelMap, m_channelMap );
        oldToNewChannelMapAdaptor.copy_structure( &newDefaultParticle[0], &m_defaultParticle[0] );
        m_defaultParticle.swap( newDefaultParticle );
    } else {
        m_defaultParticle.resize( inputChannelMap.structure_size() );
        memset( &m_defaultParticle[0], 0, inputChannelMap.structure_size() );
    }

    // set our outgoing map to this requested map
    m_channelMap = inputChannelMap;

    // set our delegate to it's own native map
    m_delegate->set_channel_map( m_delegate->get_native_channel_map() );
    m_delegateChannelMap = m_delegate->get_channel_map();

    // create native channel map for our stream. this is the delegate's native map, plus our new channels.
    m_nativeChannelMap = m_delegate->get_channel_map();
    if( !m_nativeChannelMap.has_channel( m_resultChannelName ) ) {
        if( m_resultChannelName ==
            _T( "Density" ) ) // THIS *REALLY* needs to be done with a class template. what a hack. yuck.
            m_nativeChannelMap.append_channel( m_resultChannelName, 1, frantic::channels::data_type_float16 );
        else
            m_nativeChannelMap.append_channel( m_resultChannelName, 3, frantic::channels::data_type_float16 );
    }

    // make adaptor
    // delegate provides particles in their native form, so this adaptor switches them into our requested form
    m_cma.set( m_channelMap, m_delegateChannelMap );

    // create the buffer with the correct channel map, and size it to the right number of particles.
    m_bufferedParticles = frantic::particles::particle_array( m_channelMap );
    m_bufferedParticles.resize( m_maxBufferSize );
}

size_t texture_evaluation_particle_istream::texturemap_2d_fill_particle_buffer() {
    // This function is to fill the m_bufferedParticles, and apply the 2d texture map to the color channels
    size_t newBufferSize = m_maxBufferSize;

    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> uvwAccessor =
        m_delegateChannelMap.get_cvt_accessor<frantic::graphics::vector3f>(
            frantic::strings::to_tstring( m_uvwChannelName ) );

    MFloatArray uArrayBuffer;
    MFloatArray vArrayBuffer;
    uArrayBuffer.setLength( (unsigned int)m_maxBufferSize );
    vArrayBuffer.setLength( (unsigned int)m_maxBufferSize );

    std::vector<char> particleBuffer( m_delegateChannelMap.structure_size() );
    for( size_t i = 0; i < m_maxBufferSize; ++i ) {
        if( !m_delegate->get_particle( &particleBuffer[0] ) ) {
            newBufferSize = i;
            break;
        }
        const frantic::graphics::vector3f& uvwSource = uvwAccessor( &particleBuffer[0] );
        uArrayBuffer[(unsigned int)i] =
            uvwSource.x; // only the x,y components are currently used from our uvw channel (for 2d textures).
        vArrayBuffer[(unsigned int)i] = uvwSource.y;
        // convert native channel map particle to our current channel map. this will copy it into our buffer in the
        // correct spot.
        m_cma.copy_structure( m_bufferedParticles[i], &particleBuffer[0], &m_defaultParticle[0] );
    }

    if( m_channelMap.has_channel( frantic::strings::to_tstring( m_resultChannelName ) ) )
        apply_2d_texture_evaluation( m_bufferedParticles, newBufferSize, m_mayaMaterialNodeName, uArrayBuffer,
                                     vArrayBuffer, m_resultChannelName );

    return newBufferSize;
}

size_t texture_evaluation_particle_istream::texturemap_3d_fill_particle_buffer() {
    // This function is to fill the m_bufferedParticles, and apply the 3d texture map to the color channels
    size_t newBufferSize = m_maxBufferSize;

    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> uvwAccessor =
        m_delegateChannelMap.get_cvt_accessor<frantic::graphics::vector3f>(
            frantic::strings::to_tstring( m_uvwChannelName ) );

    MFloatPointArray uvwArrayBuffer;
    uvwArrayBuffer.setLength( (unsigned int)m_maxBufferSize );

    std::vector<char> particleBuffer( m_delegateChannelMap.structure_size() );
    for( size_t i = 0; i < m_maxBufferSize; ++i ) {
        if( !m_delegate->get_particle( &particleBuffer[0] ) ) {
            newBufferSize = i;
            break;
        }
        MFloatPoint& uvwDest = uvwArrayBuffer[(unsigned int)i];
        const frantic::graphics::vector3f& uvwSource = uvwAccessor( &particleBuffer[0] );
        uvwDest.x = uvwSource.x;
        uvwDest.y = uvwSource.y;
        uvwDest.z = uvwSource.z;
        uvwDest.w = 1.0f;
        // convert native channel map particle to our current channel map. this will copy it into our buffer in the
        // correct spot.
        m_cma.copy_structure( m_bufferedParticles[i], &particleBuffer[0], &m_defaultParticle[0] );
    }

    if( m_channelMap.has_channel( frantic::strings::to_tstring( m_resultChannelName ) ) )
        apply_3d_texture_evaluation( m_bufferedParticles, newBufferSize, m_mayaMaterialNodeName, uvwArrayBuffer,
                                     m_resultChannelName );

    return newBufferSize;
}

} // namespace particles
} // namespace maya
} // namespace frantic
