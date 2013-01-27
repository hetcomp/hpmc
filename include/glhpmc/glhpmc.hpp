#pragma once
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
#include <GL/glew.h>
/**
  * \mainpage
  *
  * \author Christopher Dyken, <christopher.dyken.at.sintef.no>
  *
  * HPMC is a small library that extracts iso-surfaces of volumetric
  * data directly on the GPU, using the method described in:
  *
  * High-speed Marching Cubes using Histogram Pyramids",
  * Computer Graphics Forum 27 (8), 2008.
  *
  * It uses OpenGL to interface the GPU and assumes that the volumetric data is
  * already resident on the GPU or can be accessed through some shader code. The
  * output is a set of vertices, optionally with normal vectors, where three and
  * three vertices form triangles of the iso-surfaces. The output can be
  * directly extracted in the vertex shader for visualization, or be captured
  * into a vertex buffer object using transform feedback.
  *
  * \section usage Usage
  *
  * The public interface to HPMC is defined in \c hpmc.h
  *
  * Use of HPMC usually involves the following initialization steps:
  * - First, choose an OpenGL target version (\ref HPMCTarget). This determines
  *   which parts of the OpenGL API HPMC uses as well as to which GLSL dialect
  *   the shaders are generated in.
  *
  * - In initialization, create an HPMCConstants .
  * - Create one or more HistoPyramid instances, for each HistoPyramid:
  *   - Specify lattice and grid dimensions.
  *   - Specify the scalar field.
  *   - Create one or more traversal handles:
  *     - Acquire the traversal source code.
  *     - Build the corresponding OpenGL display shader program.
  *     - Associate the display shader program with the traversal handle.
  *
  * In the render loop, one usually have the following steps:
  * - For each HistoPyramid instance:
  *   - Setup custom fetch shader texture units (if applicable).
  *   - Trigger construction of the HistoPyramid
  * - For every traversal handle:
  *   - Setup custom fetch shader and display code texture units
  *     (if applicable).
  *   - Trigger rendering of the iso-surface.
  *
  * \subsection create_hp Creating and configuring a HistoPyramid instance
  * The first step is to create a set of constants:
  * \code
  * struct HPMCConstants* hpmc_c;
  * hpmc_c = HPMCcreateConstants();
  * \endcode
  * The set of constants contains a few textures and buffer objects that HPMC
  * needs. The data is constant and can be shared by all HPMC instances on the
  * current OpenGL context (or within the set of sharing contexts).
  *
  * The next step is to create a HistoPyramid instance:
  * \code
  * struct HPMCHistoPyramid* hpmc_h;
  * hpmc_h = HPMCcreateHistoPyramid( hpmc_c );
  * \endcode
  *
  * The HistoPyramid is tied to a particular volume configuration, which is
  * configured with the following two calls:
  * \code
  * HPMCsetLatticeSize( hpmc_h, 128, 128, 128 );
  * HPMCsetGridSize( hpmc_h, 127, 127, 127 );
  * \endcode
  * The lattice size is the number of scalar field samples along the x-, y-, and
  * z-directions.
  *
  * The grid size is the number of marching cubes cells that lie
  * in-between the lattice samples (the lattice samples are assumed to lie on
  * the corners of the marching cubes cells). The default size is grid size
  * minus one. However, unless the application provides the means to sample the
  * gradient field, forward differences are used. In this case, positions
  * outside the lattice grid might get sampled, resulting in erroneous normal
  * vectors along three sides of the domain. Reducing grid size to lattice size
  * minus two remedies this.
  *
  * The extent of the grid (in object space) is specified using
  * \code
  * HPMCsetGridExtent( hpmc_h, 1.0f, 1.0f, 1.0f );
  * \endcode
  * Positions corresponding to gridsize plus one maps to the values specified
  * as grid extent.
  *
  * \subsubsection create_hp_tex If the scalar field stored in a texture
  *
  * The final step is to specify the scalar field. If the scalar field is stored
  * in a Texture3D, we associate the texture name using:
  * \code
  * HPMCsetFieldTexture3D( hpmc_h, volume_tex, GL_FALSE );
  * \endcode
  * The scalar field is assumed to be stored in the alpha channel of the
  * texture.
  *
  * If the gradient field is known, one use
  * \code
  * HPMCsetFieldTexture3D( hpmc_h, volume_tex, GL_TRUE );
  * \endcode
  * instead. The components of the gradient is assumed to be stored in the red,
  * green, and blue channels of the texture while the scalar field is assumed to
  * be stored in the alpha channel.
  *
  * \subsubsection create_hp_code Using a custom fetch function
  *
  * On the other hand, if the scalar field is defined in terms of a snippet of
  * shader code, HPMC access this through two shader functions with the
  * following signatures:
  * \code
  * float HPMC_fetch( vec3 p );
  * vec4  HPMC_fetchGrad( vec3 );
  * \endcode
  * The \c HPMC_fetch function must always be defined. The additional
  * \c HPMC_fetchGrad only has to be defined if the gradient field is provided.
  * The source code to this function is set using either
  * \code
  * HPMCsetFieldCustom( hpmc_h, fetch_code, free_sampler, GL_FALSE );
  * \endcode
  * or
  * \code
  * HPMCsetFieldCustom( hpmc_h, fetch_code, free_sampler, GL_TRUE );
  * \endcode
  * depending on whether the gradient is provided. The \c free_sampler
  * parameter denotes a sampler unit that is not used by the fetch code and that
  * HPMC can use freely when constructing the HistoPyramid base level.
  *
  * If the fetch code uses uniform variables that must be set, the program name
  * of the HistoPyramid base level shader can be obtained using
  * \code
  * GLuint builder = HPMCgetBuilderProgram( hpmc_h );
  * \endcode
  *
  * \subsection create_tr Creating and configuring the traversal
  *
  * To extract the geometry, HPMC must traverse the HistoPyramid data structure.
  * This is done using through a traversal handle. A new traversal handle for
  * a HistoPyramid instance is created using
  * \code
  * HPMCTraversalHandle* th = HPMCcreateTraversalHandle( hpmc_h );
  * \endcode
  * using the traversal handle, we can get the shader source code needed to
  * traverse the HistoPyramid,
  * \code
  * char *traversal_code = HPMCgetTraversalShaderFunctions( th );
  * \endcode
  * This code must be included in the application's vertex shader, for example
  * in such a way:
  * \code
  * const char* sources[2] = {
  *     traversal_code,
  *     application_vertex_shader_source
  * };
  * GLuint display_vs = glCreateShader( GL_VERTEX_SHADER );
  * glShaderSource( display_vs, 2, &sources[0], NULL );
  * glCompileShader( display_vs );
  * free( traversal_code );
  * \endcode
  *
  * When the application's display shader program is compiled and linked, it
  * must be associated with the traversal handle. This is done using
  * \code
  * HPMCsetTraversalHandleProgram( th, display_program, 0, 1, 2 );
  * \endcode
  * The three last arguments are three texture samplers that HPMC may freely
  * use during traversal withouth interfering with the fetch code or the
  * application's shaders. If a custom fetch function is used, HPMC only needs
  * two texture samplers and thus the last sampler is in this case not touched
  * by HPMC.
  *
  * \section display The display loop
  *
  * \subsection display_construct HistoPyramid construction
  *
  * The first step is to analyze the scalar field for a particular iso-value
  * and build the corresponding HistoPyramid. This is done using
  * \code
  * HPMCbuildHistopyramid( hpmc_h, iso_value );
  * \endcode
  * This is only needed when the scalar field or the iso-value changes, thus,
  * if this has not changed since last frame, this step can be skipped.
  *
  * Most of the OpenGL state (except the texture unit that were specified when
  * calling \c HPMCsetField* ) is kept when the base layer is constructed, so
  * textures that the fetch function uses can be bound before calling this
  * function.
  *
  * The number of vertices in the triangulation can be queried using
  * \code
  * GLuint N = HPMCacquireNumberOfVertices( hpmc_h );
  * \endcode
  * Note that this function forces a GPU-CPU synchronization.
  *
  * \subsection display_traverse Rendering the triangles
  *
  * To render the triangles, we set up the render state and invoke
  * \code
  * HPMCextractVertices( th );
  * \endcode
  * on the traversal handle. This function triggers the actual rendering.
  *
  * HPMCextractVertices also maintains most of the OpenGL state (except the
  * shader units that were specified when calling
  * HPMCsetTraversalHandleProgram), so textures that the fetch code or the
  * display shader code uses may be bound before calling this function.
  *
  */
/** \example texture3d.cpp
  * Demonstrates basic use of HPMC. It reads a raw file with bytes describing
  * the scalar field lattice and extracts iso-surfaces for a real-time varying
  * iso-value. Gradient field is found by letting HPMC form forward differences.
  */
/** \example cayley.cpp
  * Demonstrates the using application provided fetch functions with HPMC, as
  * well as letting the application providing gradient information (avoiding the
  * use of forward differences).
  */
/** \example metaballs.cpp
  * This example provides a shader function snippet to evaluate the scalar
  * field defined by a set of metaballs. It shows how the application can
  * retrieve the linked shader programs and update uniforms for the shader
  * function snippet. It does not provide a gradient evaluation snippet, so
  * forward differences are used to determine normal vectors.
  */
/** \example transform_feedback.cpp
  * This example shows how to set up transform feedback with HPMC. There are
  * three mechanisms for this in OpenGL:
  * - The OpenGL 3.0 Core functionality. Unfortunately, most versions of GLEW
  *   has an erroneous signature for glTransformFeedbackVaryings (which
  *   originated in the specification).
  * - The GL_EXT_transform_feedback extension, which is basically the OpenGL 3.0
  *   core functionality with EXT-suffixes. This extension is supported by
  *   recent versions of the ATI Catalyst drivers.
  * - The GL_NV_transform_feedback extension is slightly more cumbersome than
  *   the two other approaches, as it requires the application to first tag
  *   varyings as active (so they don't get optimized away) before linking, and
  *   then, after linking, specify the feedback using the location of the
  *   varying variables. This extension is supported in recent versions of the
  *   NVIDIA Display Driver.
  *
  * This examples does a runtime test for the NV and EXT extensions, and run
  * a separate code path depending on this test.
  */
/** \example particles.cpp
  * Demonstrates a more involved use of the transform feedback mechanism.
  * Triangles are captured into a VBO and subjected to a geometry shader program
  * that emits particles on the extracted surface. The particles are affected by
  * initial velocity (based on the surface normal), gravity, and the gradient of
  * the scalar field defining the iso-surface.
  */

/**
  * \file hpmc.h
  *
  * \brief Library header file. Defines public interface.
  *
  * Public interface is in C, while internals are in C++.
  *
  */
/** \defgroup hpmc_public Public API
  * \{
  */


namespace glhpmc {


struct HPMCConstants;

struct HPMCIsoSurface;

struct HPMCIsoSurfaceRenderer;

/** Specifies which GL features to use and not to use. */
typedef enum {
    /** OpenGL version 2.0, shading language version 1.10.
     *
     * - gl_VertexID is faked using a vbo with indices.
     *
     */
    HPMC_TARGET_GL20_GLSL110,
    /** OpenGL version 2.1, shading language version 1.20.
     *
     * Same as \ref HPMC_TARGET_GL20_GLSL110.:
     */
    HPMC_TARGET_GL21_GLSL120,
    /** OpenGL version 3.0, shading language version 1.30.
     *
     * Same as \ref HPMC_TARGET_GL21_GLSL120, except:
     * - Use overloaded texture() instead of texture2D(), texture3D(), etc.
     * - Use gl_VertexID instead of index VBO.
     * - Use of vertex array to feed the GPGPU pass
     * - Removed use of deprecated builtin variables.
     * - Use of in/out qualified variables instead of varying variables.
     */
    HPMC_TARGET_GL30_GLSL130,
    HPMC_TARGET_GL31_GLSL140,
    HPMC_TARGET_GL32_GLSL150,
    HPMC_TARGET_GL33_GLSL330,
    HPMC_TARGET_GL40_GLSL400,
    HPMC_TARGET_GL41_GLSL410,
    HPMC_TARGET_GL42_GLSL420,
    HPMC_TARGET_GL43_GLSL430
} HPMCTarget;

typedef enum {
    HPMC_DEBUG_NONE,
    HPMC_DEBUG_STDERR,
    HPMC_DEBUG_STDERR_VERBOSE,
    HPMC_DEBUG_KHR_DEBUG,
    HPMC_DEBUG_KHR_DEBUG_VERBOSE
} HPMCDebugBehaviour;



/** Specify that the scalar field is a binary field
 *
 * The scalar field is assumed to be either 0 or 1, and the iso-value is fixed
 * at 0.5. This allows certain optimizations to be made.
 *
 * \param h   Pointer to an existing HistoPyramid instance.
 *
 */
void
HPMCsetFieldAsBinary( struct HPMCIsoSurface*  h );

/** Sets that a Texture3D shall define the scalar field lattice.
 *
 * \param h         Pointer to an existing HistoPyramid instance.
 * \param texture   GL name of 3D texture.
 * \param field     Color channel where field is stored, valid values are GL_RED
 *                  or GL_ALPHA.
 * \param gradient  Color channels where gradient is stored, valid values are
 *                  GL_RGB or GL_NONE (which implies no gradients and gradient
 *                  is found by HPMC on the fly using forward differences).
 * \returns         True on success.
 */
bool
HPMCsetFieldTexture3D( struct HPMCIsoSurface*  h,
                       GLuint                  texture,
                       GLenum                  field,
                       GLenum                  gradient );

/** Sets a custom fetch function for the lattice.
  *
  * This function sets that a custom application-provided fetch function should
  * be used to fetch samples from the lattice. The application provides a code
  * snippet with one or two fetch functions,
  * \code
  * float
  * HPMC_fetch( vec3 p )
  * {
  *     return f;
  * }
  * \endcode
  * that takes a point in [0,1]^3 and returns the scalar value, and, if
  * gradient is enabled,
  * \code
  * vec4
  * HPMC_fetchGrad( vec3 p )
  * {
  *     return vec4( dfdx, dfdy, dfdz, f );
  * }
  * \endcode
  * The coordinates of p are texel centers in normalized texture coordinates.
  *
  * If the fetch function requires that e.g. certain uniforms are set, the
  * applictaion must query HPMC for these programs and configure the shader
  * program. The fetch function may be used in two places:
  * - It is always used in the HistoPyramid buildup phase, and that shader
  *   program can be queried by \ref HPMCgetBuilderProgram.
  * - It is used in the HistoPyramid traversal phase unless the field is binary
  *   (in which case the Marching Cube case gives all required information). The
  *   traversal program is directly managed by the application.
  *
  * \param h                Pointer to an existing HistoPyramid instance.
  * \param shader_source    A string containing the custom fetch shader source.
  * \param builder_texunit  A texunit that HPMC can use during baselevel
  *                         construction that does not interfere with any
  *                         texunits that the custom fetch shader uses.
  * \param gradient         True if fetch shader provides gradients, otherwise
  *                         the gradient is approximated using forward differences.
  */
void
HPMCsetFieldCustom( struct HPMCIsoSurface*  h,
                    const char*               shader_source,
                    GLuint                    builder_texunit,
                    GLboolean                 gradient );

/** Get the shader program that inspects the field in the histopyramid build step.
 *
 * \note If the HistoPyramid is reconfigured in some way, shader programs are
 * rebuilt.
 */
GLuint
HPMCgetBuilderProgram( struct HPMCIsoSurface*  h );



/** Returns the number of vertices in the histopyramid.
  *
  * \note Must be called after HPMCbuildHistopyramd*().
  */
GLuint
HPMCacquireNumberOfVertices( struct HPMCIsoSurface* handle );


/** Create a new traversal handle instance.
  *
  * \return      A pointer to a HistoPyramid traversal handle on success, NULL
  *              on failure.
  * \sideeffect  None.
  */
struct HPMCIsoSurfaceRenderer*
HPMCcreateIsoSurfaceRenderer( struct HPMCIsoSurface* handle );

/** Destroy a traversal handle and free associated resources.
  *
  * \sideeffect None.
  */
void
HPMCdestroyIsoSurfaceRenderer( struct HPMCIsoSurfaceRenderer* th );

/** Get shader source that implements the traversal and extraction.
  *
  * \return      A fresh copy of the shader source on success, NULL on failure.
  *              It is the application's responsibility to free this memory
  *              (using free).
  * \sideeffect  None.
  */
char*
HPMCisoSurfaceRendererShaderSource( struct HPMCIsoSurfaceRenderer* th );

/** Associates a linked shader program with a traversal handle.
  *
  * \param program         A successfully linked program including the source
  *                        code provided by HPMCgetTraversalShaderFunctions in
  *                        the vertex shader.
  * \param tex_unit_work1  A unique texture unit that HPMC may use during
  *                        traversal without interfering with the rest of the
  *                        program.
  * \param tex_unit_work2  A unique texture unit that HPMC may use during
  *                        traversal without interfering with the rest of the
  *                        program.
  * \param tex_unit_work3  A unique texture unit that HPMC may use during
  *                        traversal without interfering with the rest of the
  *                        program. Not used with custom scalar field fetch
  *                        functions.
  * \return                True on success, false on failure.
  * \sideeffect            None.
  */
bool
HPMCsetIsoSurfaceRendererProgram( struct  HPMCIsoSurfaceRenderer *th,
                               GLuint  program,
                               GLuint  tex_unit_work1,
                               GLuint  tex_unit_work2,
                               GLuint  tex_unit_work3 );

/** Extract the triangles of the iso-surface
 *
 * No texture units except those specified in setTraversalHandleProgram will be
 * touched, so one may bind to other texture units that is used by a custom
 * fetch function (if used).
 *
 * \return                True on success, false on failure.
 *
 * \sideeffect None.
 */

bool
HPMCextractVertices( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation );

bool
HPMCextractVerticesTransformFeedback( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation );

bool
HPMCextractVerticesTransformFeedbackNV( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation );

bool
HPMCextractVerticesTransformFeedbackEXT( struct HPMCIsoSurfaceRenderer* th, GLboolean flip_orientation );

} // of namespace glhpmc

/** \} */
