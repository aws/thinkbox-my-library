// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/util.hpp>

namespace frantic {
namespace maya {

class MPxParticleStream_impl : public MPxParticleStream {

  private:
    particle_stream_source* m_particle_source;

  public:
    MPxParticleStream_impl();
    virtual ~MPxParticleStream_impl();

    virtual void copy( const MPxData& src );

    virtual MTypeId typeId() const;

    virtual MString name() const;

    virtual frantic::particles::streams::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& objectSpace,
                             const MDGContext& context = MDGContext::fsNormal ) const;

    virtual frantic::particles::streams::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                               const MDGContext& context = MDGContext::fsNormal ) const;

    virtual unsigned long getVersion() const;

    virtual void setParticleSource( particle_stream_source* prtObj );

    virtual particle_stream_source* getParticleSource() const;
};

#pragma region MPxParticleStream
const MTypeId MPxParticleStream::id( 0x0011748c );
const MString MPxParticleStream::typeName( "ParticleStreamMPxData" );

void* MPxParticleStream::creator() { return new MPxParticleStream_impl; }
#pragma endregion

//////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long MPxParticleStream_impl::getVersion() const { return 1; }

MPxParticleStream_impl::MPxParticleStream_impl()
    : m_particle_source( NULL ) {}

MPxParticleStream_impl::~MPxParticleStream_impl() {}

void MPxParticleStream_impl::copy( const MPxData& src ) {
    if( src.typeId() == MPxParticleStream_impl::id ) {
        const MPxParticleStream* psrc = frantic::maya::mpx_cast<const MPxParticleStream*>( &src );
        if( psrc != NULL ) {
            this->setParticleSource( psrc->getParticleSource() );
            return;
        }
    }
    throw std::runtime_error( "MPxParticleStream::copy failed. src MPxData is not MPxParticleStream" );
}

MTypeId MPxParticleStream_impl::typeId() const { return MPxParticleStream_impl::id; }

MString MPxParticleStream_impl::name() const { return MPxParticleStream_impl::typeName; }

frantic::particles::streams::particle_istream_ptr
MPxParticleStream_impl::getRenderParticleStream( const frantic::graphics::transform4f& objectSpace,
                                                 const MDGContext& context ) const {
    return m_particle_source->getRenderParticleStream( objectSpace, context );
}

frantic::particles::streams::particle_istream_ptr
MPxParticleStream_impl::getViewportParticleStream( const frantic::graphics::transform4f& objectSpace,
                                                   const MDGContext& context ) const {
    return m_particle_source->getViewportParticleStream( objectSpace, context );
}

void MPxParticleStream_impl::setParticleSource( particle_stream_source* prtObj ) { m_particle_source = prtObj; }

particle_stream_source* MPxParticleStream_impl::getParticleSource() const { return m_particle_source; }

} // namespace maya
} // namespace frantic
