// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <maya/M3dView.h> // for OpenGL includes

#include <frantic/maya/graphics/opengl.hpp>

using namespace frantic::graphics;

namespace frantic {
namespace maya {
namespace graphics {

void gl_draw_wireframe( const frantic::geometry::trimesh3& mesh ) {
    const frantic::tstring visibilityChannelName( _T("FaceEdgeVisibility") );

    frantic::geometry::const_trimesh3_face_channel_accessor<boost::int8_t> visibilityAcc;

    if( mesh.has_face_channel( visibilityChannelName ) ) {
        visibilityAcc = mesh.get_face_channel_accessor<boost::int8_t>( visibilityChannelName );
    }

    glPushAttrib( GL_CURRENT_BIT );
    glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );

    glEnableClientState( GL_VERTEX_ARRAY );

    glBegin( GL_LINES );

    for( std::size_t i = 0; i < mesh.face_count(); ++i ) {
        const vector3& face = mesh.get_face( i );

        const vector3f& v0 = mesh.get_vertex( face[0] );
        const vector3f& v1 = mesh.get_vertex( face[1] );
        const vector3f& v2 = mesh.get_vertex( face[2] );

        const boost::int8_t visibility = visibilityAcc.valid() ? visibilityAcc[i] : 0x07;
        if( visibility & 0x01 ) {
            glVertex3f( v0.x, v0.y, v0.z );
            glVertex3f( v1.x, v1.y, v1.z );
        }
        if( visibility & 0x02 ) {
            glVertex3f( v1.x, v1.y, v1.z );
            glVertex3f( v2.x, v2.y, v2.z );
        }
        if( visibility & 0x04 ) {
            glVertex3f( v2.x, v2.y, v2.z );
            glVertex3f( v0.x, v0.y, v0.z );
        }
    }

    glEnd();

    glPopClientAttrib();
    glPopAttrib();
}

void gl_draw( const frantic::geometry::trimesh3& mesh ) {

    glPushAttrib( GL_CURRENT_BIT );
    glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );

    glEnableClientState( GL_VERTEX_ARRAY );

    glBegin( GL_TRIANGLES );

    for( size_t i = 0; i < mesh.face_count(); ++i ) {
        const vector3& face = mesh.get_face( i );

        const vector3f& v0 = mesh.get_vertex( face[0] );
        const vector3f& v1 = mesh.get_vertex( face[1] );
        const vector3f& v2 = mesh.get_vertex( face[2] );
        glVertex3f( v0.x, v0.y, v0.z );
        glVertex3f( v1.x, v1.y, v1.z );
        glVertex3f( v2.x, v2.y, v2.z );
    }

    glEnd();

    /*
    glVertexPointer( 3, GL_FLOAT, 0, &mesh.get_vertex(0)[0] );
    glDrawElements( GL_TRIANGLES, mesh.face_count() * 3, GL_UNSIGNED_INT, &mesh.get_face(0)[0] );
    */

    glPopClientAttrib();
    glPopAttrib();
}

void gl_draw_box_wireframe( const frantic::graphics::boundbox3f& box ) {
    if( box.is_empty() ) {
        return;
    }

    const vector3f v[] = { box.minimum(), box.maximum() };

    glPushAttrib( GL_CURRENT_BIT );
    glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );

    glEnableClientState( GL_VERTEX_ARRAY );

    for( int z = 0; z < 2; ++z ) {
        glBegin( GL_LINE_LOOP );

        glVertex3f( v[0].x, v[0].y, v[z].z );
        glVertex3f( v[1].x, v[0].y, v[z].z );
        glVertex3f( v[1].x, v[1].y, v[z].z );
        glVertex3f( v[0].x, v[1].y, v[z].z );

        glEnd();
    }

    glBegin( GL_LINES );

    for( int x = 0; x < 2; ++x ) {
        for( int y = 0; y < 2; ++y ) {
            for( int z = 0; z < 2; ++z ) {
                glVertex3f( v[x].x, v[y].y, v[z].z );
            }
        }
    }

    glEnd();

    glPopClientAttrib();
    glPopAttrib();
}

} // namespace graphics
} // namespace maya
} // namespace frantic
