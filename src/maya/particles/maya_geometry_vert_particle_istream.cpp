// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/particles/maya_geometry_vert_particle_istream.hpp>

#include <frantic/channels/channel_map_adaptor.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace frantic::maya::particles;

const frantic::tstring maya_geometry_vert_particle_istream::s_positionChannel = _T( "Position" );
const frantic::tstring maya_geometry_vert_particle_istream::s_velocityChannel = _T( "Velocity" );
const frantic::tstring maya_geometry_vert_particle_istream::s_normalChannel = _T( "Normal" );
const frantic::tstring maya_geometry_vert_particle_istream::s_idChannel = _T( "ID" );
const frantic::tstring maya_geometry_vert_particle_istream::s_colorChannel = _T( "Color" );
const frantic::tstring maya_geometry_vert_particle_istream::s_uvChannel = _T( "TextureCoord" );

maya_geometry_vert_particle_istream::maya_geometry_vert_particle_istream( MPlug meshPlug )
    : m_mesh( boost::make_shared<frantic::geometry::trimesh3>() ) {
    init_stream( meshPlug );
    set_channel_map( m_nativeMap );
}

maya_geometry_vert_particle_istream::~maya_geometry_vert_particle_istream() {}

void maya_geometry_vert_particle_istream::close() { m_mesh.reset(); }

frantic::tstring maya_geometry_vert_particle_istream::name() const { return _T( "maya_mesh_particle_istream" ); }

std::size_t maya_geometry_vert_particle_istream::particle_size() const { return m_outMap.structure_size(); }

boost::int64_t maya_geometry_vert_particle_istream::particle_count() const { return m_totalParticles; }

boost::int64_t maya_geometry_vert_particle_istream::particle_index() const { return m_currentParticle; }

boost::int64_t maya_geometry_vert_particle_istream::particle_count_left() const {
    return m_totalParticles - m_currentParticle;
}

boost::int64_t maya_geometry_vert_particle_istream::particle_progress_count() const { return m_totalParticles; }

boost::int64_t maya_geometry_vert_particle_istream::particle_progress_index() const { return m_currentParticle; }

void maya_geometry_vert_particle_istream::set_channel_map( const frantic::channels::channel_map& particleChannelMap ) {
    std::vector<char> newDefaultParticle( particleChannelMap.structure_size() );
    if( newDefaultParticle.size() > 0 ) {
        if( m_defaultParticle.size() > 0 ) {
            frantic::channels::channel_map_adaptor defaultAdaptor( particleChannelMap, m_outMap );
            defaultAdaptor.copy_structure( &newDefaultParticle[0], &m_defaultParticle[0] );
        }
    }

    m_defaultParticle.swap( newDefaultParticle );

    m_outMap = particleChannelMap;
    init_accessors( particleChannelMap );
}

const frantic::channels::channel_map& maya_geometry_vert_particle_istream::get_channel_map() const { return m_outMap; }

const frantic::channels::channel_map& maya_geometry_vert_particle_istream::get_native_channel_map() const {
    return m_nativeMap;
}

void maya_geometry_vert_particle_istream::set_default_particle( char* rawParticleBuffer ) {
    memcpy( &m_defaultParticle[0], rawParticleBuffer, m_outMap.structure_size() );
}

bool maya_geometry_vert_particle_istream::get_particle( char* rawParticleBuffer ) {

    if( !m_mesh ) {
        throw std::runtime_error( "maya_geometry_vert_particle_istream::get_particle: Tried to read particle from "
                                  "stream after it was already closed" );
    }

    if( m_currentParticle >= m_totalParticles ) {
        return false;
    }

    if( m_particleAccessors.position.is_valid() ) {
        const frantic::graphics::vector3f position =
            m_mesh->get_vertex( static_cast<std::size_t>( m_currentParticle ) );
        m_particleAccessors.position.set( rawParticleBuffer, position );
    }

    if( m_particleAccessors.id.is_valid() ) {
        m_particleAccessors.id.set( rawParticleBuffer, static_cast<int>( m_currentParticle ) );
    }

    if( m_particleAccessors.velocity.is_valid() ) {
        const frantic::graphics::vector3f velocity = get_vertex_data( m_vertexAccessors.velocity, m_currentParticle );
        m_particleAccessors.velocity.set( rawParticleBuffer, velocity );
    }

    if( m_particleAccessors.normal.is_valid() ) {
        const frantic::graphics::vector3f normal = get_vertex_data( m_vertexAccessors.normal, m_currentParticle );
        m_particleAccessors.normal.set( rawParticleBuffer, normal );
    }

    if( m_particleAccessors.color.is_valid() ) {
        const frantic::graphics::vector3f color = get_vertex_data( m_vertexAccessors.color, m_currentParticle );
        m_particleAccessors.color.set( rawParticleBuffer, color );
    }

    if( m_particleAccessors.uv.is_valid() ) {
        const frantic::graphics::vector3f uv = get_vertex_data( m_vertexAccessors.uv, m_currentParticle );
        m_particleAccessors.uv.set( rawParticleBuffer, uv );
    }

    ++m_currentParticle;
    if( m_currentParticle > m_totalParticles ) {
        close();
    }

    return true;
}

bool maya_geometry_vert_particle_istream::get_particles( char* buffer, std::size_t& numParticles ) {
    for( std::size_t i = 0; i < numParticles; ++i ) {
        if( !this->get_particle( buffer ) ) {
            numParticles = i;
            return false;
        }
        buffer += m_outMap.structure_size();
    }

    return true;
}

void maya_geometry_vert_particle_istream::init_stream( MPlug meshPlug ) {
    frantic::maya::geometry::copy_maya_mesh( meshPlug, *m_mesh, true, true, true, true, true );

    fill_vertex_to_face_and_corner_map();

    m_currentParticle = 0;
    m_totalParticles = m_mesh->vertex_count();

    m_nativeMap.define_channel<frantic::graphics::vector3f>( s_positionChannel );
    m_nativeMap.define_channel<int>( s_idChannel );

    if( m_mesh->has_vertex_channel( s_velocityChannel ) ) {
        m_vertexAccessors.velocity =
            m_mesh->get_vertex_channel_accessor<frantic::graphics::vector3f>( s_velocityChannel );
        m_nativeMap.define_channel<frantic::graphics::vector3f>( s_velocityChannel );
    }

    if( m_mesh->has_vertex_channel( s_normalChannel ) ) {
        m_vertexAccessors.normal = m_mesh->get_vertex_channel_accessor<frantic::graphics::vector3f>( s_normalChannel );
        m_nativeMap.define_channel<frantic::graphics::vector3f>( s_normalChannel );
    }

    if( m_mesh->has_vertex_channel( s_colorChannel ) ) {
        m_vertexAccessors.color = m_mesh->get_vertex_channel_accessor<frantic::graphics::vector3f>( s_colorChannel );
        m_nativeMap.define_channel<frantic::graphics::vector3f>( s_colorChannel );
    }

    if( m_mesh->has_vertex_channel( s_uvChannel ) ) {
        m_vertexAccessors.uv = m_mesh->get_vertex_channel_accessor<frantic::graphics::vector3f>( s_uvChannel );
        m_nativeMap.define_channel<frantic::graphics::vector3f>( s_uvChannel );
    }

    m_nativeMap.end_channel_definition();
}

void maya_geometry_vert_particle_istream::init_accessors( const frantic::channels::channel_map& pcm ) {
    m_particleAccessors.position.reset();
    m_particleAccessors.velocity.reset();
    m_particleAccessors.normal.reset();
    m_particleAccessors.id.reset();
    m_particleAccessors.color.reset();
    m_particleAccessors.uv.reset();

    if( pcm.has_channel( s_positionChannel ) ) {
        m_particleAccessors.position = pcm.get_cvt_accessor<frantic::graphics::vector3f>( s_positionChannel );
    }

    if( pcm.has_channel( s_velocityChannel ) && m_vertexAccessors.velocity.valid() ) {
        m_particleAccessors.velocity = pcm.get_cvt_accessor<frantic::graphics::vector3f>( s_velocityChannel );
    }

    if( pcm.has_channel( s_normalChannel ) && m_vertexAccessors.normal.valid() ) {
        m_particleAccessors.normal = pcm.get_cvt_accessor<frantic::graphics::vector3f>( s_normalChannel );
    }

    if( pcm.has_channel( s_idChannel ) ) {
        m_particleAccessors.id = pcm.get_cvt_accessor<int>( s_idChannel );
    }

    if( pcm.has_channel( s_colorChannel ) && m_vertexAccessors.color.valid() ) {
        m_particleAccessors.color = pcm.get_cvt_accessor<frantic::graphics::vector3f>( s_colorChannel );
    }

    if( pcm.has_channel( s_uvChannel ) && m_vertexAccessors.uv.valid() ) {
        m_particleAccessors.uv = pcm.get_cvt_accessor<frantic::graphics::vector3f>( s_uvChannel );
    }
}

void maya_geometry_vert_particle_istream::fill_vertex_to_face_and_corner_map() {
    m_vertexToFaceAndCorner.clear();
    m_vertexToFaceAndCorner.resize( m_mesh->vertex_count() );

    for( int faceIndex = 0; faceIndex < m_mesh->face_count(); ++faceIndex ) {
        const frantic::graphics::vector3 face = m_mesh->get_face( faceIndex );
        for( int corner = 0; corner < 3; ++corner ) {
            const int vertIndex = face[corner];
            m_vertexToFaceAndCorner[vertIndex].set( faceIndex, corner );
        }
    }
}

frantic::graphics::vector3f
maya_geometry_vert_particle_istream::get_vertex_data( vertex_vector_acc_t& acc, boost::int64_t vertex,
                                                      frantic::graphics::vector3f fallback ) {
    if( acc.valid() ) {
        if( acc.has_custom_faces() ) {
            face_and_corner mapping = m_vertexToFaceAndCorner[static_cast<std::size_t>( vertex )];
            if( mapping.is_valid() ) {
                const std::size_t faceIndex = static_cast<std::size_t>( mapping.face );
                const std::size_t corner = static_cast<std::size_t>( mapping.corner );
                return ( acc )[acc.face( faceIndex )[corner]];
            }
            throw std::runtime_error( "maya_geometry_vert_particle_istream::get_vertex_data: invalid mapping" );
        } else {
            return ( acc )[static_cast<std::size_t>( vertex )];
        }
    } else {
        return fallback;
    }
}