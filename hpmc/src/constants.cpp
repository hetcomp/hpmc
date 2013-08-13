/* -*- mode: C++; tab-width:4; c-basic-offset: 4; indent-tabs-mode:nil -*-
 ***********************************************************************
 *
 *  File: constants.cpp
 *
 *  Created: 24. June 2009
 *
 *  Version: $Id: $
 *
 *  Authors: Christopher Dyken <christopher.dyken@sintef.no>
 *
 *  This file is part of the HPMC library.
 *  Copyright (C) 2009 by SINTEF.  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  ("GPL") version 2 as published by the Free Software Foundation.
 *  See the file LICENSE.GPL at the root directory of this source
 *  distribution for additional information about the GNU GPL.
 *
 *  For using HPMC with software that can not be combined with the
 *  GNU GPL, please contact SINTEF for aquiring a commercial license
 *  and support.
 *
 *  SINTEF, Pb 124 Blindern, N-0314 Oslo, Norway
 *  http://www.sintef.no
 *********************************************************************/

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <hpmc.h>
#include <hpmc_internal.h>

using std::vector;
using std::cerr;
using std::endl;

#define remapCode( code ) (    \
    ((((code)>>0)&0x1)<<0) |   \
    ((((code)>>1)&0x1)<<1) |   \
    ((((code)>>4)&0x1)<<2) |   \
    ((((code)>>5)&0x1)<<3) |   \
    ((((code)>>3)&0x1)<<4) |   \
    ((((code)>>2)&0x1)<<5) |   \
    ((((code)>>7)&0x1)<<6) |   \
    ((((code)>>6)&0x1)<<7) )


// -----------------------------------------------------------------------------
struct HPMCConstants*
HPMCcreateConstants( GLint max_gl_major, GLint max_gl_minor )
{
    if( !HPMCcheckGL( __FILE__, __LINE__ ) ) {
#ifdef DEBUG
        cerr << "HPMC error: createConstants called with GL errors." << endl;
#endif
        return NULL;
    }

    // Determine GL version
    GLint gl_major, gl_minor;
    glGetIntegerv( GL_MAJOR_VERSION, &gl_major );
    glGetIntegerv( GL_MINOR_VERSION, &gl_minor );

    if( gl_major > max_gl_major ) {
        gl_major = max_gl_major;
        gl_minor = max_gl_minor;
    }
    else if( (gl_major == max_gl_major) && (gl_minor > max_gl_minor ) ) {
        gl_minor = max_gl_minor;
    }

//    gl_major = 2;
//    gl_minor = 0;


    if( gl_major < 2 ) {
        // Insufficient GL version
#ifdef DEBUG
        cerr << "HPMC error: At least GL version 2.0 is required "
             << "(system reports version " << gl_major
             << "." <<gl_minor << ")" << endl;
#endif
        return NULL;
    }
    else if( gl_major < 3 ) {
        if( !GLEW_EXT_framebuffer_object ) {
#ifdef DEBUG
            cerr << "GL version less than 3.0 and EXT_framebuffer_object is missing." << std::endl;
            return NULL;
#endif
        }
        if( !GLEW_ARB_color_buffer_float ) {
#ifdef DEBUG
            cerr << "GL version less than 3.0 and ARB_color_buffer_float is missing." << std::endl;
            return NULL;
#endif
        }
    }

    struct HPMCConstants *s = new HPMCConstants;
    s->m_enumerate_vbo = 0;
    s->m_edge_decode_tex = 0;
    s->m_edge_decode_normal_tex = 0;
    s->m_vertex_count_tex = 0;
    s->m_gpgpu_quad_vbo = 0;


    if( gl_major == 2 ) {
        if( gl_minor == 0 ) {
            s->m_target = HPMC_TARGET_GL20_GLSL110;
        }
        else {
            s->m_target = HPMC_TARGET_GL21_GLSL120;
        }
    }
    else if( gl_major == 3 ) {
        if( gl_minor == 0 ) {
            s->m_target = HPMC_TARGET_GL30_GLSL130;
        }
        else if( gl_minor == 1 ) {
            s->m_target = HPMC_TARGET_GL31_GLSL140;
        }
        else if( gl_minor == 2 ) {
            s->m_target = HPMC_TARGET_GL32_GLSL150;
        }
        else {
            s->m_target = HPMC_TARGET_GL33_GLSL330;
        }
    }
    else if( gl_major == 4 ) {
        if( gl_minor == 0 ) {
            s->m_target = HPMC_TARGET_GL40_GLSL400;
        }
        else if( gl_minor == 1 ) {
            s->m_target = HPMC_TARGET_GL41_GLSL410;
        }
        else if( gl_minor == 2 ) {
            s->m_target = HPMC_TARGET_GL42_GLSL420;
        }
        else {
            s->m_target = HPMC_TARGET_GL43_GLSL430;
        }
    }
    else {
        s->m_target = HPMC_TARGET_GL43_GLSL430;
    }

#ifdef DEBUG
    std::cerr << "HPMC uses target OpenGL ";
    switch( s->m_target) {
    case HPMC_TARGET_GL20_GLSL110:
        std::cerr << "2.0";
        break;
    case HPMC_TARGET_GL21_GLSL120:
        std::cerr << "2.1";
        break;
    case HPMC_TARGET_GL30_GLSL130:
        std::cerr << "3.0";
        break;
    case HPMC_TARGET_GL31_GLSL140:
        std::cerr << "3.1";
        break;
    case HPMC_TARGET_GL32_GLSL150:
        std::cerr << "3.2";
        break;
    case HPMC_TARGET_GL33_GLSL330:
        std::cerr << "3.3";
        break;
    case HPMC_TARGET_GL40_GLSL400:
        std::cerr << "4.0";
        break;
    case HPMC_TARGET_GL41_GLSL410:
        std::cerr << "4.1";
        break;
    case HPMC_TARGET_GL42_GLSL420:
        std::cerr << "4.2";
        break;
    case HPMC_TARGET_GL43_GLSL430:
        std::cerr << "4.3";
        break;
    default:
        std::cerr << "???";
    }
    std::cerr << "." << std::endl;
#endif


    // --- store state ---------------------------------------------------------
    glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );
    glPushAttrib( GL_TEXTURE_BIT );

    // --- build enumeration VBO, used to spawn a batch of vertices  -----------
    s->m_enumerate_vbo_n = 3*1000;
    glGenBuffers( 1, &s->m_enumerate_vbo );
    glBindBuffer( GL_ARRAY_BUFFER, s->m_enumerate_vbo );
    glBufferData( GL_ARRAY_BUFFER,
                  3*sizeof(GLfloat)*s->m_enumerate_vbo_n,
                  NULL,
                  GL_STATIC_DRAW );
    GLfloat* ptr =
            reinterpret_cast<GLfloat*>( glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY ) );

    for(int i=0; i<s->m_enumerate_vbo_n; i++) {
        *ptr++ = static_cast<GLfloat>( i );
        *ptr++ = 0.0f;
        *ptr++ = 0.0f;
    }
    glUnmapBuffer( GL_ARRAY_BUFFER );

    // --- build edge decode table ---------------------------------------------

    std::vector<GLfloat> edge_normals( 256*6*4 );

    // for each marching cubes case
    for(int j=0; j<256; j++) {

        // for each triangle in a case (why 6, should be 5?)
        for(int i=0; i<6; i++) {

            // Pick out the three indices defining a triangle. Snap illegal
            // indices to zero for simplicity (the data will never be used).
            // Then, create pointers to the appropriate vertex positions.
            GLfloat* vp[3];
            for(int k=0; k<3; k++) {
                int ix = std::max( 0, HPMC_triangle_table[j][ std::min(15,3*i+k) ] );
                vp[k] = &( HPMC_midpoint_table[ ix ][0] );
            }

            GLfloat u[3], v[3];
            for(int k=0; k<3; k++) {
                u[k] = vp[2][k]-vp[0][k];
                v[k] = vp[1][k]-vp[0][k];
            }
            GLfloat n[3];
            n[0] = u[1]*v[2] - u[2]*v[1];
            n[1] = u[2]*v[0] - u[0]*v[2];
            n[2] = u[0]*v[1] - u[1]*v[0];
            GLfloat s = 0.5f*std::sqrt( n[0]*n[0] + n[1]*n[1] + n[2]*n[2] );
            for(int k=0; k<3; k++) {
                float tmp = s*n[k]+0.5f;
                if( tmp < 0.f ) tmp  = 0.000001f;
                if( tmp >= 1.f ) tmp = 0.999999f;
                edge_normals[ 4*(6*remapCode(j)+i) + k ] = tmp;
            }
            edge_normals[ 4*(6*remapCode(j)+i) + 3 ] = 0.f;
        }
    }
    
    
    vector<GLfloat> edge_decode( 256*16*4 );
    vector<GLfloat> edge_decode_normal( 256*16*4 );

    for(size_t j=0; j<256; j++ ) {
        for(size_t i=0; i<16; i++) {
            for(size_t k=0; k<4; k++)
                edge_decode[4*16*remapCode(j) + 4*i+k ]
                        = HPMC_edge_table[ HPMC_triangle_table[j][i] != -1 ? HPMC_triangle_table[j][i] : 0 ][k];
        }
    }

    for(int j=0; j<256; j++ ) {
        for(int i=0; i<15; i++) {
            for(int k=0; k<4; k++ ) {
                edge_decode_normal[ 4*(16*j+i) +k ] = edge_decode[ 4*(16*j+i) + k ]
                                                    + edge_normals[ 4*(6*j+i/3 ) + k ];
            }
        }
    }
    
    
    glGenTextures( 1, &s->m_edge_decode_tex );
    glBindTexture( GL_TEXTURE_2D, s->m_edge_decode_tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
    glTexImage2D( GL_TEXTURE_2D, 0,
                  GL_RGBA32F_ARB, 16, 256,0,
                  GL_RGBA, GL_FLOAT,
                  edge_decode.data() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    
    glGenTextures( 1, &s->m_edge_decode_normal_tex );
    glBindTexture( GL_TEXTURE_2D, s->m_edge_decode_normal_tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
    glTexImage2D( GL_TEXTURE_2D, 0,
                  GL_RGBA32F_ARB, 16, 256,0,
                  GL_RGBA, GL_FLOAT,
                  edge_decode_normal.data() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    
    // --- build vertex count table --------------------------------------------
    vector<GLfloat> tricount( 256 );
    for(size_t j=0; j<256; j++) {
        size_t count;
        for(count=0; count<16; count++) {
            if( HPMC_triangle_table[j][count] == -1 ) {
                break;
            }
        }

        tricount[ remapCode(j) ] = static_cast<GLfloat>( count );
    }

    glGenTextures( 1, &s->m_vertex_count_tex );
    glBindTexture( GL_TEXTURE_1D, s->m_vertex_count_tex );
    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0 );
    glTexImage1D( GL_TEXTURE_1D, 0,
                  GL_ALPHA32F_ARB, 256, 0,
                  GL_ALPHA, GL_FLOAT,
                  &tricount[0] );
    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    // --- build GPGPU quad vbo ------------------------------------------------
    glGenBuffers( 1, &s->m_gpgpu_quad_vbo );
    glBindBuffer( GL_ARRAY_BUFFER, s->m_gpgpu_quad_vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(GLfloat)*3*4, &HPMC_gpgpu_quad_vertices[0], GL_STATIC_DRAW );

    // --- restore state -------------------------------------------------------
    glPopClientAttrib();
    glPopAttrib();

    if( !HPMCcheckGL( __FILE__, __LINE__ ) ) {
#ifdef DEBUG
        cerr << "HPMC error: createConstants created GL errors." << endl;
#endif
        HPMCdestroyConstants( s );
        delete s;

        return NULL;
    }
    return s;
}

// -----------------------------------------------------------------------------
void
HPMCdestroyConstants( struct HPMCConstants* s )
{
    if( s == NULL ) {
        return ;
    }

    if( !HPMCcheckGL( __FILE__, __LINE__ ) ) {
#ifdef DEBUG
        cerr << "HPMC error: destroyConstants called with GL errors." << endl;
#endif
        return;
    }

    if( s->m_enumerate_vbo != 0 ) {
        glDeleteBuffers( 1, &s->m_enumerate_vbo );
    }

    if( s->m_edge_decode_tex != 0 ) {
        glDeleteTextures( 1, &s->m_edge_decode_tex );
    }

    if( s->m_edge_decode_normal_tex != 0 ) {
        glDeleteTextures( 1, &s->m_edge_decode_normal_tex );
    }
    
    if( s->m_vertex_count_tex != 0 ) {
        glDeleteTextures( 1, &s->m_vertex_count_tex );
    }

    if( s->m_gpgpu_quad_vbo != 0u ) {
        glDeleteBuffers( 1, &s->m_gpgpu_quad_vbo );
    }

    if( !HPMCcheckGL( __FILE__, __LINE__ ) ) {
#ifdef DEBUG
        cerr << "HPMC error: destroyConstants introduced GL errors." << endl;
#endif
        return;
    }

    delete s;
}
