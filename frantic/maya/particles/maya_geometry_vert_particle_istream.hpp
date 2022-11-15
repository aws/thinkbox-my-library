// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/channels/channel_map.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/geometry/trimesh3_named_channels.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

#include <frantic/maya/geometry/mesh.hpp>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <maya/MPlug.h>

namespace frantic {
namespace maya {
namespace particles {

class maya_geometry_vert_particle_istream : public frantic::particles::streams::particle_istream {
  private:
    typedef frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> vector_channel_acc_t;
    typedef frantic::channels::channel_cvt_accessor<int> integral_channel_acc_t;
    typedef frantic::geometry::trimesh3_vertex_channel_accessor<frantic::graphics::vector3f> vertex_vector_acc_t;

    const static frantic::tstring s_positionChannel;
    const static frantic::tstring s_velocityChannel;
    const static frantic::tstring s_normalChannel;
    const static frantic::tstring s_idChannel;
    const static frantic::tstring s_colorChannel;
    const static frantic::tstring s_uvChannel;

    boost::shared_ptr<frantic::geometry::trimesh3> m_mesh;
    frantic::channels::channel_map m_nativeMap;
    frantic::channels::channel_map m_outMap;

    std::vector<char> m_defaultParticle;

    boost::int64_t m_totalParticles;
    boost::int64_t m_currentParticle;

    struct {
        vector_channel_acc_t position;
        vector_channel_acc_t velocity;
        vector_channel_acc_t normal;
        integral_channel_acc_t id;
        vector_channel_acc_t color;
        vector_channel_acc_t uv;
    } m_particleAccessors;

    struct {
        vertex_vector_acc_t velocity;
        vertex_vector_acc_t normal;
        vertex_vector_acc_t color;
        vertex_vector_acc_t uv;
    } m_vertexAccessors;

    struct face_and_corner {
        int face;
        int corner;
        face_and_corner()
            : face( -1 )
            , corner( -1 ) {}
        void set( int face, int corner ) {
            this->face = face;
            this->corner = corner;
        }
        bool is_valid() { return face >= 0; }
    };

    std::vector<face_and_corner> m_vertexToFaceAndCorner;

  public:
    maya_geometry_vert_particle_istream( MPlug meshPlug );

    virtual ~maya_geometry_vert_particle_istream();

    virtual void close();

    virtual frantic::tstring name() const;

    virtual std::size_t particle_size() const;
    virtual boost::int64_t particle_count() const;
    virtual boost::int64_t particle_index() const;
    virtual boost::int64_t particle_count_left() const;
    virtual boost::int64_t particle_progress_count() const;
    virtual boost::int64_t particle_progress_index() const;

    virtual void set_channel_map( const frantic::channels::channel_map& particleChannelMap );
    virtual const frantic::channels::channel_map& get_channel_map() const;
    virtual const frantic::channels::channel_map& get_native_channel_map() const;

    virtual void set_default_particle( char* rawParticleBuffer );

    virtual bool get_particle( char* rawParticleBuffer );
    virtual bool get_particles( char* buffer, std::size_t& numParticles );

  private:
    void init_stream( MPlug meshPlug );
    void init_accessors( const frantic::channels::channel_map& pcm );
    void fill_vertex_to_face_and_corner_map();
    frantic::graphics::vector3f
    get_vertex_data( vertex_vector_acc_t& acc, boost::int64_t vertex,
                     frantic::graphics::vector3f fallback = frantic::graphics::vector3f( 0.0f, 0.0f, 0.0f ) );
};

} // namespace particles
} // namespace maya
} // namespace frantic