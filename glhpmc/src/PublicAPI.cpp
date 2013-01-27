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
#include <algorithm>
#include <string>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <glhpmc/glhpmc.hpp>
#include <glhpmc/glhpmc_internal.hpp>
#include <glhpmc/Constants.hpp>
#include <glhpmc/IsoSurface.hpp>
#include <glhpmc/IsoSurfaceRenderer.hpp>
#include <glhpmc/Logger.hpp>

namespace glhpmc {
    static const std::string package = "HPMC.publicAPI";


#if 0
void
HPMCsetFieldAsBinary( struct HPMCIsoSurface*  h )
{
    Logger log( h->constants(), package + ".setFieldAsBinary", true );
    h->oldField().m_binary = true;
    h->taint();
}


bool
HPMCsetFieldTexture3D( struct HPMCIsoSurface*  h,
                       GLuint                  texture,
                       GLenum                  field,
                       GLenum                  gradient )
{
    if( h == NULL ) {
        return false;
    }
    Logger log( h->constants(), package + ".setFieldTexture3D", true );
    if( texture == 0 ) {
        log.errorMessage( "Illegal texture name" );
        return false;
    }
    if( (field != GL_RED) && (field != GL_ALPHA) ) {
        log.errorMessage( "Illegal field enum" );
        return false;
    }
    if( (gradient != GL_RGB) && (gradient != GL_NONE) ) {
        log.errorMessage( "Illegal gradient enum" );
        return false;
    }
    if( (gradient == GL_RGB) && (field == GL_RED) ) {
        log.errorMessage( "field and gradient channels are not distinct" );
        return false;
    }

    h->oldField().m_tex = texture;
    log.setObjectLabel( GL_TEXTURE, h->oldField().m_tex, "field texture 3D" );

    if( (h->oldField().m_mode != HPMC_VOLUME_LAYOUT_TEXTURE_3D ) ||
        (h->oldField().m_tex_field_channel != field ) ||
        (h->oldField().m_tex_gradient_channels != gradient ) )
    {
        h->oldField().m_mode = HPMC_VOLUME_LAYOUT_TEXTURE_3D;
        h->oldField().m_tex_field_channel = field;
        h->oldField().m_tex_gradient_channels = gradient;
        h->m_hp_build.m_tex_unit_1 = 0;
        h->m_hp_build.m_tex_unit_2 = 1;
        h->taint();
        if( h->constants()->debugBehaviour() != HPMC_DEBUG_NONE ) {
            std::stringstream o;
            o << "field mode=TEX3D";
            switch( field ) {
            case GL_RED:
                o << ", field=RED";
                break;
            case GL_ALPHA:
                o << ", field=ALPHA";
                break;
            default:
                break;
            }
            switch( gradient ) {
            case GL_RGB:
                o << ", gradient=RGB";
                break;
            case GL_NONE:
                o << ", gradient=NONE";
                break;
            default:
                break;
            }
            o << ", build.texunits={"
              << h->m_hp_build.m_tex_unit_1 << ", "
              << h->m_hp_build.m_tex_unit_2 << " }";
            log.debugMessage( o.str() );
        }
    }
    return true;
}


void
HPMCsetFieldCustom( struct HPMCIsoSurface*  h,
                    const char*               shader_source,
                    GLuint                    builder_texunit,
                    GLboolean                 gradient )
{
    Logger log( h->constants(), package + ".setFieldCustom", true );
    h->oldField().m_mode = HPMC_VOLUME_LAYOUT_CUSTOM;
    h->oldField().m_shader_source = shader_source;
    h->oldField().m_tex = 0;
    h->oldField().m_tex_field_channel = GL_RED;
    h->oldField().m_tex_gradient_channels = (gradient==GL_TRUE?GL_RGB:GL_NONE);
    h->m_hp_build.m_tex_unit_1 = builder_texunit;
    h->m_hp_build.m_tex_unit_2 = builder_texunit+1;
    h->taint();
}
#endif


GLuint
HPMCgetBuilderProgram( struct HPMCIsoSurface*  h )
{
    if( h == NULL ) {
        return 0;
    }
    Logger log( h->constants(), package + ".getBuilderProgram", true );
    h->untaint();
    return h->baseLevelBuilder().program();
}



GLuint
HPMCacquireNumberOfVertices( struct HPMCIsoSurface* h )
{
    if( h == NULL ) {
        return 0;
    }
    Logger log( h->constants(), package + ".acquireNumberOfVertices", true );
    return h->vertexCount();
}


struct HPMCIsoSurfaceRenderer*
HPMCcreateIsoSurfaceRenderer( struct HPMCIsoSurface* h )
{
    if( h == NULL ) {
        return NULL;
    }
    Logger log( h->constants(), "HPMC.publicAPI.createIsoSurfaceRenderer", true );
    GLint old_viewport[4];
    GLuint old_pbo, old_fbo, old_prog;
    glGetIntegerv( GL_VIEWPORT, old_viewport );
    glGetIntegerv( GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&old_prog) );
    glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, reinterpret_cast<GLint*>(&old_pbo) );
    glGetIntegerv( GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&old_fbo) );
    if( !h->untaint() ) {
        log.errorMessage( "Failed to untaint histopyramid" );
        return NULL;
    }
    glBindBuffer( GL_PIXEL_PACK_BUFFER, old_pbo );
    glBindFramebuffer( GL_FRAMEBUFFER, old_fbo );
    glViewport( old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3] );
    glUseProgram( old_prog );
    return new HPMCIsoSurfaceRenderer(h);
}

// -----------------------------------------------------------------------------
void
HPMCdestroyIsoSurfaceRenderer( struct HPMCIsoSurfaceRenderer* th )
{
    if( th == NULL ) {
        return;
    }
    Logger log( th->m_handle->constants(), package + ".destroyIsoSurfaceRenderer", true );
    delete th;
}

char*
HPMCisoSurfaceRendererShaderSource( struct HPMCIsoSurfaceRenderer* th )
{
    if( th == NULL ) {
        return NULL;
    }
    Logger log( th->m_handle->constants(), package + ".createIsoSurfaceRenderer", true );
    GLint old_viewport[4];
    GLuint old_pbo, old_fbo, old_prog;
    glGetIntegerv( GL_VIEWPORT, old_viewport );
    glGetIntegerv( GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&old_prog) );
    glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, reinterpret_cast<GLint*>(&old_pbo) );
    glGetIntegerv( GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&old_fbo) );
    if( !th->m_handle->untaint() ) {
        log.errorMessage( "Failed to untaint histopyramid" );
        return NULL;
    }
    glBindBuffer( GL_PIXEL_PACK_BUFFER, old_pbo );
    glBindFramebuffer( GL_FRAMEBUFFER, old_fbo );
    glViewport( old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3] );
    glUseProgram( old_prog );

    return strdup( th->extractionSource().c_str() );
}


bool
HPMCsetIsoSurfaceRendererProgram( struct  HPMCIsoSurfaceRenderer *th,
                                  GLuint  program,
                                  GLuint  tex_unit_work1,
                                  GLuint  tex_unit_work2,
                                  GLuint  tex_unit_work3 )
{
    if( th == NULL ) {
        return false;
    }
    GLuint old_prog;
    glGetIntegerv( GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&old_prog) );
    bool retval = th->setProgram( program, tex_unit_work1, tex_unit_work2, tex_unit_work3 );
    glUseProgram( old_prog );
    return retval;
}

// -----------------------------------------------------------------------------
bool
HPMCextractVertices( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation )
{
    if( th == NULL ) {
        return false;
    }
    GLint curr_prog;
    glGetIntegerv( GL_CURRENT_PROGRAM, &curr_prog );
    th->draw( 0, flip_orientation==GL_TRUE );
    glUseProgram( curr_prog );
    return true;
}

// -----------------------------------------------------------------------------
bool
HPMCextractVerticesTransformFeedback( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation )
{
    if( th == NULL ) {
        return false;
    }
    GLint curr_prog;
    glGetIntegerv( GL_CURRENT_PROGRAM, &curr_prog );
    th->draw( 1, flip_orientation==GL_TRUE );
    glUseProgram( curr_prog );
    return true;
}

// -----------------------------------------------------------------------------
bool
HPMCextractVerticesTransformFeedbackNV( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation )
{
    if( th == NULL ) {
        return false;
    }
    GLint curr_prog;
    glGetIntegerv( GL_CURRENT_PROGRAM, &curr_prog );
    th->draw( 2, flip_orientation==GL_TRUE );
    glUseProgram( curr_prog );
    return true;
}

// -----------------------------------------------------------------------------
bool
HPMCextractVerticesTransformFeedbackEXT( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation )
{
    if( th == NULL ) {
        return false;
    }
    GLint curr_prog;
    glGetIntegerv( GL_CURRENT_PROGRAM, &curr_prog );
    th->draw( 3, flip_orientation==GL_TRUE );
    glUseProgram( curr_prog );
    return true;
}

} // of namespace glhpmc
