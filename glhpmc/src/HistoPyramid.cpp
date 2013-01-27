/* Copyright STIFTELSEN SINTEF 2012
 *
 * This file is part of the HPMC Library.
 *
 * Author(s): Christopher Dyken, <christopher.dyken@sintef.no>
 *
 * HPMC is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * HPMC is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * HPMC.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cmath>
#include <glhpmc/glhpmc_internal.hpp>
#include <iostream>
#include <sstream>
#include <glhpmc/HistoPyramid.hpp>
#include <glhpmc/GPGPUQuad.hpp>
#include <glhpmc/Constants.hpp>
#include <glhpmc/Logger.hpp>



namespace glhpmc {
    namespace resources {
        extern std::string reduction_first_fs_110;
        extern std::string reduction_first_fs_130;
        extern std::string reduction_upper_fs_110;
        extern std::string reduction_upper_fs_130;
    } // of namespace resources
    static const std::string package = "HPMC.HistoPyramid";
    using std::endl;


HPMCHistoPyramid::HPMCHistoPyramid(HPMCConstants *constants )
    : m_constants( constants ),
      m_size(0),
      m_size_l2(0),
      m_tex(0),
      m_top_pbo(0)
{
    Logger log( m_constants, package + ".constructor" );

    glGenTextures( 1, &m_tex );
    log.setObjectLabel( GL_TEXTURE, m_tex, "histopyramid" );
    glGenBuffers( 1, &m_top_pbo );
    log.setObjectLabel( GL_BUFFER, m_top_pbo, "histopyramid top readback" );
    glBindBuffer( GL_PIXEL_PACK_BUFFER, m_top_pbo );
    glBufferData( GL_PIXEL_PACK_BUFFER,
                  sizeof(GLfloat)*4,
                  NULL,
                  GL_DYNAMIC_READ );
    glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
}

bool
HPMCHistoPyramid::init()
{
    Logger log( m_constants, package + ".init" );

    bool retval = true;

    if( m_constants->target() < HPMC_TARGET_GL30_GLSL130 ) {
        // --- GLSL 110 path ---------------------------------------------------

        // Build base-level reduction
        GLuint fs_0 = HPMCcompileShader( m_constants->versionString() +
                                         resources::reduction_first_fs_110,
                                         GL_FRAGMENT_SHADER );
        if( fs_0 != 0 ) {
            m_reduce1_program = glCreateProgram();
            glAttachShader( m_reduce1_program, m_constants->gpgpuQuad().passThroughVertexShader() );
            glAttachShader( m_reduce1_program, fs_0 );
            glDeleteShader( fs_0 );
            m_constants->gpgpuQuad().configurePassThroughVertexShader( m_reduce1_program );
            if( HPMClinkProgram( m_reduce1_program ) ) {
                m_reduce1_loc_delta = glGetUniformLocation( m_reduce1_program, "HPMC_delta" );
                m_reduce1_loc_hp_tex = glGetUniformLocation( m_reduce1_program, "HPMC_histopyramid" );
            }
            else {
                log.errorMessage( "Failed to link first reduction program" );
                glDeleteProgram( m_reduce1_program );
                m_reduce1_program = 0;
                return false;
            }
        }
        else {
            log.errorMessage( "Failed to compile first reduction fragment shader" );
            return false;
        }

        // Build upper-level reduction
        GLuint fs_n = HPMCcompileShader( m_constants->versionString() +
                                         resources::reduction_upper_fs_110,
                                         GL_FRAGMENT_SHADER );
        if( fs_n != 0 ) {
            m_reducen_program = glCreateProgram();
            glAttachShader( m_reducen_program, m_constants->gpgpuQuad().passThroughVertexShader() );
            glAttachShader( m_reducen_program, fs_n );
            glDeleteShader( fs_n );
            m_constants->gpgpuQuad().configurePassThroughVertexShader( m_reducen_program );
            if( HPMClinkProgram( m_reducen_program ) ) {
                m_reducen_loc_delta = glGetUniformLocation( m_reducen_program, "HPMC_delta" );
                m_reducen_loc_hp_tex = glGetUniformLocation( m_reducen_program, "HPMC_histopyramid" );
            }
            else {
                log.errorMessage( "Failed to link subsequent reduction program" );
                glDeleteProgram( m_reducen_program );
                m_reducen_program = 0;
                glDeleteProgram( m_reduce1_program );
                m_reduce1_program = 0;
                return false;
            }
        }
        else {
            glDeleteProgram( m_reduce1_program );
            m_reduce1_program = 0;
            return false;
        }
    }
    else {
        // --- GLSL 130 path ---------------------------------------------------

        // Build base-level reduction
        GLuint fs_0 = HPMCcompileShader( m_constants->versionString() +
                                         resources::reduction_first_fs_130,
                                         GL_FRAGMENT_SHADER );
        if( fs_0 != 0 ) {
            m_reduce1_program = glCreateProgram();
            glAttachShader( m_reduce1_program, m_constants->gpgpuQuad().passThroughVertexShader() );
            glAttachShader( m_reduce1_program, fs_0 );
            glDeleteShader( fs_0 );
            m_constants->gpgpuQuad().configurePassThroughVertexShader( m_reduce1_program );
            if( HPMClinkProgram( m_reduce1_program ) ) {
                glBindFragDataLocation(  m_reduce1_program, 0, "fragment" );
                m_reduce1_loc_level = glGetUniformLocation( m_reduce1_program, "HPMC_src_level" );
                m_reduce1_loc_hp_tex = glGetUniformLocation( m_reduce1_program, "HPMC_histopyramid" );
            }
            else {
                log.errorMessage( "Failed to link first reduction program" );
                glDeleteProgram( m_reduce1_program );
                m_reduce1_program = 0;
                return false;
            }
        }
        else {
            log.errorMessage( "Failed to compile first reduction fragment shader" );
            return false;
        }

        // Build upper-level reduction
        GLuint fs_n = HPMCcompileShader( m_constants->versionString() +
                                         resources::reduction_upper_fs_130,
                                         GL_FRAGMENT_SHADER );
        if( fs_n != 0 ) {
            m_reducen_program = glCreateProgram();
            glAttachShader( m_reducen_program, m_constants->gpgpuQuad().passThroughVertexShader() );
            glAttachShader( m_reducen_program, fs_n );
            glDeleteShader( fs_n );
            m_constants->gpgpuQuad().configurePassThroughVertexShader( m_reducen_program );
            if( HPMClinkProgram( m_reducen_program ) ) {
                glBindFragDataLocation(  m_reducen_program, 0, "fragment" );
                m_reducen_loc_level = glGetUniformLocation( m_reducen_program, "HPMC_src_level" );
                m_reducen_loc_hp_tex = glGetUniformLocation( m_reducen_program, "HPMC_histopyramid" );
            }
            else {
                log.errorMessage( "Failed to link subsequent reduction program" );
                glDeleteProgram( m_reducen_program );
                m_reducen_program = 0;
                glDeleteProgram( m_reduce1_program );
                m_reduce1_program = 0;
                return false;
            }
        }
        else {
            glDeleteProgram( m_reduce1_program );
            m_reduce1_program = 0;
            return false;
        }
    }
    return retval;
}

bool
HPMCHistoPyramid::build( GLint tex_unit_a )
{
    bool retval = true;
    Logger log( m_constants, package + ".build" );


    // level 1
    if( m_size_l2 >= 1 ) {
        if( m_reduce1_program == 0 ) {
            log.errorMessage( "Failed to build first reduction program" );
            retval = false;
        }
        else {
            glActiveTexture( GL_TEXTURE0 + tex_unit_a );
            glBindTexture( GL_TEXTURE_2D, m_tex );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );

            glUseProgram( m_reduce1_program );
            glUniform1i( m_reduce1_loc_hp_tex, tex_unit_a );
            if( m_constants->target() < HPMC_TARGET_GL30_GLSL130 ) {
                glUniform2f( m_reduce1_loc_delta, -0.5f/m_size, 0.5f/m_size );
                glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, m_fbos[1] );
            }
            else {
                glUniform1i( m_reduce1_loc_level, 0 );
                glBindFramebuffer( GL_FRAMEBUFFER, m_fbos[1] );
            }
            glViewport( 0, 0, m_size/2, m_size/2 );
            m_constants->gpgpuQuad().render();

            // levels 2 and up
            if( m_size_l2 >= 2 ) {
                if( m_reducen_program == 0 ) {
                    log.errorMessage( "Subsequent reduction program has not been successfully built." );
                    retval = false;
                }
                else {
                    glUseProgram( m_reducen_program );
                    glUniform1i( m_reducen_loc_hp_tex, tex_unit_a );
                    if( m_constants->target() < HPMC_TARGET_GL30_GLSL130 ) {
                        for( GLsizei m=2; m<= m_size_l2; m++ ) {
                            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, m-1 );
                            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m-1 );

                            glUniform2f( m_reducen_loc_delta,
                                         -0.5f/(1<<(m_size_l2+1-m)),
                                         0.5f/(1<<(m_size_l2+1-m)) );

                            glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, m_fbos[m] );
                            glViewport( 0, 0, 1<<(m_size_l2-m), 1<<(m_size_l2-m) );
                            m_constants->gpgpuQuad().render();
                        }
                    }
                    else {
                        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
                        for( GLsizei m=2; m<= m_size_l2; m++ ) {
                            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m-1 );
                            glUniform1i( m_reduce1_loc_level, m-1 );
                            glBindFramebuffer( GL_FRAMEBUFFER, m_fbos[m] );
                            glViewport( 0, 0, 1<<(m_size_l2-m), 1<<(m_size_l2-m) );
                            m_constants->gpgpuQuad().render();
                        }
                    }
                }
            }
        }
    }

    // readback of top element
    glBindBuffer( GL_PIXEL_PACK_BUFFER, m_top_pbo );
    glBindTexture( GL_TEXTURE_2D, m_tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, m_size_l2 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_size_l2 );
    glGetTexImage( GL_TEXTURE_2D, m_size_l2,GL_RGBA, GL_FLOAT, NULL );
    glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
    m_top_count_updated = false;

    return retval;
}


HPMCHistoPyramid::~HPMCHistoPyramid()
{
    Logger log( m_constants, package + ".destructor" );
    glDeleteBuffers( 1, &m_top_pbo );
    glDeleteTextures( 1, &m_tex );
    if( !m_fbos.empty() ) {
        if( m_constants->target() < HPMC_TARGET_GL30_GLSL130 ) {
            glDeleteFramebuffersEXT( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );
        }
        else {
            glDeleteFramebuffers( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );
        }
    }
}

GLsizei
HPMCHistoPyramid::count()
{
    if( !m_top_count_updated ) {
        Logger log( m_constants, package + ".count" );
        GLfloat mem[4];
        glBindBuffer( GL_PIXEL_PACK_BUFFER, m_top_pbo );
        glGetBufferSubData( GL_PIXEL_PACK_BUFFER, 0, sizeof(GLfloat)*4, mem );
        glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
        m_top_count = static_cast<GLsizei>( floorf( mem[0] ) )
                    + static_cast<GLsizei>( floorf( mem[1] ) )
                    + static_cast<GLsizei>( floorf( mem[2] ) )
                    + static_cast<GLsizei>( floorf( mem[3] ) );
        m_top_count_updated = true;
    }
    return m_top_count;
}

bool
HPMCHistoPyramid::configure( GLsizei size_l2 )
{
    Logger log( m_constants, package + ".configure");


    bool retval = true;

    m_top_count = 0;
    m_top_count_updated = false;
    m_size_l2 = size_l2;
    m_size = 1<<m_size_l2;

    // resize texture
    glBindTexture( GL_TEXTURE_2D, m_tex );
    glTexImage2D( GL_TEXTURE_2D, 0,
                  GL_RGBA32F_ARB,
                  m_size, m_size, 0,
                  GL_RGBA, GL_FLOAT,
                  NULL );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_size_l2 );
    glGenerateMipmap( GL_TEXTURE_2D );

    // release old fbos and set up new
    if( m_constants->target() < HPMC_TARGET_GL30_GLSL130 ) {
        if( !m_fbos.empty() ) {
            glDeleteFramebuffersEXT( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );
        }
        m_fbos.resize( m_size_l2 + 1 );
        glGenFramebuffersEXT( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );

        for( GLuint m=0; m<m_fbos.size(); m++) {
            glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, m_fbos[m] );
            glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT,
                                       GL_COLOR_ATTACHMENT0_EXT,
                                       GL_TEXTURE_2D,
                                       m_tex,
                                       m );
            glDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
            GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER_EXT );
            if( status != GL_FRAMEBUFFER_COMPLETE_EXT ) {
                std::stringstream o;
                o << "Framebuffer for HP level " << m << " of " << m_size_l2 << " is incomplete";
                log.errorMessage( o.str() );
                retval = false;
            }
        }
    }
    else {
        if( !m_fbos.empty() ) {
            glDeleteFramebuffers( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );
        }
        m_fbos.resize( m_size_l2 + 1 );
        glGenFramebuffers( static_cast<GLsizei>( m_fbos.size() ), m_fbos.data() );

        for( GLuint m=0; m<m_fbos.size(); m++) {
            glBindFramebuffer( GL_FRAMEBUFFER, m_fbos[m] );
            glFramebufferTexture2D( GL_FRAMEBUFFER,
                                    GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_2D,
                                    m_tex,
                                    m );
            glDrawBuffer( GL_COLOR_ATTACHMENT0 );
            GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
            if( status != GL_FRAMEBUFFER_COMPLETE ) {
                std::stringstream o;
                o << "Framebuffer for HP level " << m << " of " << m_size_l2 << " is incomplete";
                log.errorMessage( o.str() );
                retval = false;
            }
        }
    }

    if( log.doDebug() ) {
        std::stringstream o;
        o << "histopyramid.size = 2^" << m_size_l2 << " = " << m_size;
        log.debugMessage( o.str() );
    }
    return retval;
}

} // of namespace glhpmc
