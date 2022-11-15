// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>

namespace frantic {
namespace maya {
namespace particles {

enum maya_texture_type_t { TEXTURE_TYPE_2D, TEXTURE_TYPE_3D, TEXTURE_TYPE_UNSUPPORTED };

/**
 * Utility function to determine the texture type of a given maya material node.
 * @param mayaMaterialNodeName The name of the scene material. eg "checker1".
 */
maya_texture_type_t get_texture_type( const std::string& mayaMaterialNodeName );

/**
 * Evaulates a 2d texture map based on provided U,V coordinates and assigns the result to a channel.
 * @param pArray The particle array to be used. This array does not need to have a UVW channel in it. That is provided
 * separately. It does have to have the "outputChannelName" channel though. The array must be at least "numParticles" in
 * length.
 * @param numParticles The number of particles in pArray to process. Generally this value is set to pArray.size(), but
 * can be smaller.
 * @param mayaMaterialNodeName The name of the scene material. eg "checker1".
 * @param uArray An array of corresponding "u" coordinates. This array should be the same size as pArray.
 * @param vArray An array of corresponding "v" coordinates. This array should be the same size as pArray.
 * @param outputChannelName The channel in pArray to which the resulting color will be assigned. It can be arity 3
 * (color) or 1 (greyscale).
 */
void apply_2d_texture_evaluation( frantic::particles::particle_array& pArray, size_t numParticles,
                                  const std::string& mayaMaterialNodeName, const MFloatArray& uArray,
                                  const MFloatArray& vArray, const std::string& outputChannelName );

/**
 * Evaulates a 3d texture map based on provided U,V coordinates and assigns the result to a channel.
 * @param pArray The particle array to be used. This array does not need to have a UVW channel in it. That is provided
 * separately. It does have to have the "outputChannelName" channel though. The array must be at least "numParticles" in
 * length.
 * @param numParticles The number of particles in pArray to process. Generally this value is set to pArray.size(), but
 * can be smaller.
 * @param mayaMaterialNodeName The name of the scene material. eg "checker1".
 * @param uvwArray An array of corresponding "uvw" coordinates. This array should be the same size as pArray. The actual
 * point values in the array should be x=sourceU,y=sourceV,z=sourceW,w=1.0.
 * @param outputChannelName The channel in pArray to which the resulting color will be assigned. It can be arity 3
 * (color) or 1 (greyscale).
 */
void apply_3d_texture_evaluation( frantic::particles::particle_array& pArray, size_t numParticles,
                                  const std::string& mayaMaterialNodeName, const MFloatPointArray& uvwArray,
                                  const std::string& outputChannelName );

/**
 * A stream that provides Maya texture map evaluation.
 */
class texture_evaluation_particle_istream : public frantic::particles::streams::particle_istream {
  protected:
    // variables from delegate
    boost::shared_ptr<frantic::particles::streams::particle_istream> m_delegate;
    boost::int64_t m_particleIndex;

    frantic::tstring m_uvwChannelName;
    frantic::tstring m_resultChannelName;

    std::string m_mayaMaterialNodeName;
    bool m_is2dTexture; // true=2d texture, false=3d texture

    // our current channel map
    frantic::channels::channel_map m_channelMap;
    frantic::channels::channel_map_adaptor m_cma; // m_delegateChannelMap to m_channelMap

    // the native channel map
    frantic::channels::channel_map m_nativeChannelMap;
    frantic::channels::channel_map m_delegateChannelMap;

    // for the buffered particles
    frantic::particles::particle_array m_bufferedParticles;
    size_t m_maxBufferSize; // user set
    size_t m_currentBufferSize;
    size_t m_bufferedParticlesIndex;

    // our default particle
    std::vector<char> m_defaultParticle;

  public:
    /**
     * Creates a stream from a delegate that sets a channel to be the result of a Maya texture map evaluation.
     * @param pin This incoming particle stream
     * @param mayaMaterialNodeName The name of the Maya material node that is to be evaluated.
     * @param uvwChannelName The name of the channel in the delegate particle stream that is to be used as a UVW channel
     * for the material.
     * @param resultChannelName The name of the channel to which the resulting colors will be placed. Most often this
     * will be "Color".
     * @param bufferSize Internally, this stream buffers chunks of particles in memory. This parameter determines the
     * number of particles to hold in memory at once. This is needed because Maya must process particles in batch. Small
     * buffer means low performance, but less memory usuage. Large buffers mean high performance, but more memory usage.
     */
    texture_evaluation_particle_istream( boost::shared_ptr<frantic::particles::streams::particle_istream> pin,
                                         const std::string& mayaMaterialNodeName,
                                         const frantic::tstring& uvwChannelName,
                                         const frantic::tstring resultChannelName, size_t bufferSize = 100000 );

    virtual ~texture_evaluation_particle_istream() {}

    void close() { m_delegate->close(); }
    boost::int64_t particle_count() const { return m_delegate->particle_count(); }
    boost::int64_t particle_index() const { return m_particleIndex; }
    boost::int64_t particle_count_left() const {
        boost::int64_t particleCount = m_delegate->particle_count();
        if( particleCount == -1 )
            return -1;
        return particleCount - m_particleIndex;
    }
    boost::int64_t particle_progress_count() const { return particle_count(); }
    boost::int64_t particle_progress_index() const { return particle_index(); }
    boost::int64_t particle_count_guess() const { return m_delegate->particle_count_guess(); }
    frantic::tstring name() const { return m_delegate->name(); }
    std::size_t particle_size() const { return m_channelMap.structure_size(); }

    void set_channel_map( const frantic::channels::channel_map& particleChannelMap );
    void set_default_particle( char* rawParticleBuffer );
    const frantic::channels::channel_map& get_channel_map() const;
    const frantic::channels::channel_map& get_native_channel_map() const;

    bool get_particle( char* outParticleBuffer );
    bool get_particles( char* buffer, std::size_t& numParticles );

  private:
    void init_channel_map( const frantic::channels::channel_map& inputChannelMap );
    size_t texturemap_2d_fill_particle_buffer();
    size_t texturemap_3d_fill_particle_buffer();
};

} // namespace particles
} // namespace maya
} // namespace frantic
