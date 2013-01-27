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
#include <iostream>
#include <sstream>
#include <glhpmc/Constants.hpp>
#include <glhpmc/Logger.hpp>

// Missing from GLEW 1.9.0, should appear in 1.9.1
extern "C" {
    void glPopDebugGroup();
}

namespace glhpmc {

Logger::Logger( const HPMCConstants* constants, const std::string& where, bool force_check )
    : m_constants( constants ),
      m_where( where ),
      m_force_check( force_check )
{
    HPMCDebugBehaviour debug = m_constants->debugBehaviour();
    if( (m_force_check && debug == HPMC_DEBUG_STDERR ) || (debug == HPMC_DEBUG_STDERR_VERBOSE ) ) {
        GLenum error = glGetError();
        if( error != GL_NO_ERROR ) {
            warningMessage( "Invoked with GL errors" );
            while( error != GL_NO_ERROR ) {
                warningMessage( glErrorString( error ) );
                error = glGetError();
            }
        }
    }
    else if( debug == HPMC_DEBUG_KHR_DEBUG_VERBOSE ) {
       /* glPushDebugGroup( GL_DEBUG_SOURCE_THIRD_PARTY,
                          0,
                          m_where.length()+1, m_where.c_str() );*/
    }
}


Logger::~Logger()
{
    HPMCDebugBehaviour debug = m_constants->debugBehaviour();
    if( (m_force_check && debug == HPMC_DEBUG_STDERR ) || (debug == HPMC_DEBUG_STDERR_VERBOSE ) ) {
        GLenum error = glGetError();
        if( error != GL_NO_ERROR ) {
            errorMessage( "Generated GL errors" );
            while( error != GL_NO_ERROR ) {
                errorMessage( glErrorString( error ) );
                error = glGetError();
            }
        }
    }
    else if( debug == HPMC_DEBUG_KHR_DEBUG_VERBOSE ) {
        //glPopDebugGroup();
    }
}

void
Logger::debugMessage( const std::string& msg )
{
    switch( m_constants->debugBehaviour() ) {
    case HPMC_DEBUG_NONE:
        break;
    case HPMC_DEBUG_STDERR:
        break;
    case HPMC_DEBUG_STDERR_VERBOSE:
        std::cerr << "[D] " << m_where << ": " << msg << std::endl;
        break;
    case HPMC_DEBUG_KHR_DEBUG:
        break;
    case HPMC_DEBUG_KHR_DEBUG_VERBOSE:
        glDebugMessageInsert( GL_DEBUG_SOURCE_THIRD_PARTY,
                              GL_DEBUG_TYPE_OTHER,
                              0,
                              GL_DEBUG_SEVERITY_LOW,
                              msg.length()+1,
                              msg.c_str() );
        break;
    }

}

void
Logger::warningMessage( const std::string& msg )
{
    switch( m_constants->debugBehaviour() ) {
    case HPMC_DEBUG_NONE:
        break;
    case HPMC_DEBUG_STDERR:
    case HPMC_DEBUG_STDERR_VERBOSE:
        std::cerr << "[W] " << m_where << ": " << msg << std::endl;
        break;
    case HPMC_DEBUG_KHR_DEBUG:
    case HPMC_DEBUG_KHR_DEBUG_VERBOSE:
        glDebugMessageInsert( GL_DEBUG_SOURCE_THIRD_PARTY,
                              GL_DEBUG_TYPE_OTHER,
                              0,
                              GL_DEBUG_SEVERITY_MEDIUM,
                              msg.length()+1,
                              msg.c_str() );
        break;
    }
}

void
Logger::errorMessage( const std::string& msg )
{
    switch( m_constants->debugBehaviour() ) {
    case HPMC_DEBUG_NONE:
        break;
    case HPMC_DEBUG_STDERR:
    case HPMC_DEBUG_STDERR_VERBOSE:
        std::cerr << "[E] " << m_where << ": " << msg << std::endl;
        break;
    case HPMC_DEBUG_KHR_DEBUG:
    case HPMC_DEBUG_KHR_DEBUG_VERBOSE:
        glDebugMessageInsert( GL_DEBUG_SOURCE_THIRD_PARTY,
                              GL_DEBUG_TYPE_OTHER,
                              0,
                              GL_DEBUG_SEVERITY_HIGH,
                              msg.length()+1,
                              msg.c_str() );
        break;
    }

}

const std::string
Logger::glErrorString( GLenum error ) const
{
    switch( error ) {
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
        break;
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
        break;
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION"; break;
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW"; break;
    case GL_OUT_OF_MEMORY:                  return "GL_OUT_OF_MEMORY"; break;
    case GL_TABLE_TOO_LARGE:                return "GL_TABLE_TOO_LARGE"; break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:  return "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
    default:
        return "Unknown error";
    }
}

void
Logger::setObjectLabel( GLenum identifier, GLuint name, const std::string label )
{
    switch( m_constants->debugBehaviour() ) {
    case HPMC_DEBUG_NONE:
        break;
    case HPMC_DEBUG_STDERR:
        break;
    case HPMC_DEBUG_STDERR_VERBOSE:
        if( 1 ) {
            std::stringstream o;
            switch( identifier ) {
            case GL_BUFFER: o << "Buffer "; break;
            case GL_SHADER: o << "Shader "; break;
            case GL_PROGRAM: o << "Program "; break;
            case GL_VERTEX_ARRAY: o << "Vertex array "; break;
            case GL_QUERY: o << "Query "; break;
            case GL_PROGRAM_PIPELINE: o << "Program pipeline "; break;
            case GL_TRANSFORM_FEEDBACK: o << "Transform feedback "; break;
            case GL_SAMPLER: o << "Sampler "; break;
            case GL_TEXTURE: o << "Texture "; break;
            case GL_RENDERBUFFER: o << "Render buffer "; break;
            case GL_FRAMEBUFFER: o << "Framebuffer "; break;
            default:
                o << "Unidentified object "; break;
            }
            o << name << " is " << label;
            debugMessage( o.str() );
        }
        break;
    case HPMC_DEBUG_KHR_DEBUG:
        glObjectLabel( identifier, name, label.length()+1, label.c_str() );
        break;

    }
}

} // of namespace glhpmc
