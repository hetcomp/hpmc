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
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cuda_gl_interop.h>
#include <builtin_types.h>
#include <sstream>
#include <vector>
#include <iomanip>
#include <iostream>
#include "../common/common.hpp"
#include <cuhpmc/Constants.hpp>
#include <cuhpmc/FieldGlobalMemUChar.hpp>
#include <cuhpmc/GLFieldUCharBuffer.hpp>
#include <cuhpmc/IsoSurface.hpp>
#include <cuhpmc/GLIsoSurface.hpp>
#include <cuhpmc/TriangleVertexWriter.hpp>
#include <cuhpmc/GLWriter.hpp>

using std::cerr;
using std::endl;

int                             volume_size_x       = 128;
int                             volume_size_y       = 128;
int                             volume_size_z       = 128;
float                           iso                 = 0.5f;

cuhpmc::Constants*              constants           = NULL;
unsigned char*                  field_data_dev      = NULL;
cuhpmc::AbstractField*          field               = NULL;
cuhpmc::AbstractIsoSurface*     iso_surface         = NULL;
cuhpmc::AbstractWriter*         writer              = NULL;

GLuint                          surface_vao         = 0;
GLuint                          surface_vbo         = 0;
GLsizei                         surface_vbo_n       = 0;
cudaGraphicsResource*           surface_resource    = NULL;
cudaStream_t                    stream              = 0;
float*                          surface_cuda_d      = NULL;
cudaEvent_t                     pre_buildup         = 0;
cudaEvent_t                     post_buildup        = 0;
float                           buildup_ms          = 0.f;

cudaEvent_t                     pre_write           = 0;
cudaEvent_t                     post_write          = 0;
float                           write_ms            = 0.f;

uint                            runs                = 0;

bool                            profile             = false;
bool                            gl_direct_draw      = true;
GLuint                          gl_field_buffer     = 0;

template<class type, bool clamp, bool half_float>
__global__
void
bumpyCayley( type* __restrict__ output,
             const uint field_x,
             const uint field_y,
             const uint field_z,
             const uint field_row_pitch,
             const uint field_slice_pitch,
             const uint by )
{
    uint ix = blockIdx.x*blockDim.x + threadIdx.x;
    uint iy = (blockIdx.y%by)*blockDim.y + threadIdx.y;
    uint iz = blockIdx.y/by;
    if( ix < field_x && iy < field_y && iz < field_z ) {
        float x = 2.f*(float)ix/(float)field_x-1.f;
        float y = 2.f*(float)iy/(float)field_y-1.f;
        float z = 2.f*(float)iz/(float)field_z-1.f;
        float f = 16.f*x*y*z + 4.f*x*x + 4.f*y*y + 4.f*z*z - 1.f;
//                  + 0.2f*sinf(33.f*x)*cosf(39.1f*y)*sinf(37.3f*z)
//                  + 0.1f*sinf(75.f*x)*cosf(75.1f*y)*sinf(71.3f*z);
/*
        - 0.6*sinf(25.1*x)*cosf(23.2*y)*sinf(21*z)
                  + 0.4*sinf(41.1*x*y)*cosf(47.2*y)*sinf(45*z)
                  - 0.2*sinf(111.1*x*y)*cosf(117.2*y)*sinf(115*z);
*/
//        f = sin(f);
        if( clamp ) {
            f = 255.f*f;
            if( f > 255 ) f = 255.f;
            if( f < 0 ) f = 0.f;
        }
        if( half_float ) {
            output[ ix + iy*field_row_pitch + iz*field_slice_pitch ] = __float2half_rn( f );
        }
        else {
            output[ ix + iy*field_row_pitch + iz*field_slice_pitch ] = f;
        }
    }
}


void
printHelp( const std::string& appname )
{
    //       --------------------------------------------------------------------------------
    cerr << "HPMC demo application that visualizes 1-16xyz-4x^2-4y^2-4z^2=iso."<<endl<<endl;
    cerr << "Usage: " << appname << " [options] xsize [ysize zsize] "<<endl<<endl;
    cerr << "where: xsize    The number of samples in the x-direction."<<endl;
    cerr << "       ysize    The number of samples in the y-direction."<<endl;
    cerr << "       zsize    The number of samples in the z-direction."<<endl;
    cerr << "Example usage:"<<endl;
    cerr << "    " << appname << " 64"<< endl;
    cerr << "    " << appname << " 64 128 64"<< endl;
    cerr << endl;
    cerr << "Options specific for this app:" << std::endl;
    cerr << "    --device <int>  Specify which CUDA device to use." << endl;
    cerr << "    --gl-direct     Enable direct rendering in OpenGL (i.e., do not store" << endl;
    cerr << "                    geomtry in a buffer)." << endl;
    cerr << "    --no-gl-direct  Disable direct rendering in OpenGL (i.e., geometry is stored" << endl;
    cerr << "                    in a buffer by CUDA, which OpenGL then renders)." << endl;
    cerr << "    --profile       Enable profiling of CUDA passes." << endl;
    cerr << "    --no-profile    Disable profiling of CUDA passes." << endl;
    cerr << endl;
    printOptions();
}


void
init( int argc, char** argv )
{
    int device = -1;
    for( int i=1; i<argc; ) {
        int eat = 0;
        std::string arg( argv[i] );
        if( (arg == "--device") && (i+1)<argc ) {
            device = atoi( argv[i+1] );
            eat = 2;
        }
        else if( (arg == "--gl-direct" ) ) {
            gl_direct_draw = true;
            eat = 1;
        }
        else if( (arg == "--no-gl-direct" ) ) {
            gl_direct_draw = false;
            eat = 1;
        }
        else if( (arg == "--profile" ) ) {
            profile = true;
            eat = 1;
        }
        else if( (arg == "--no-profile" ) ) {
            profile = false;
            eat = 1;
        }

        if( eat ) {
            argc = argc - eat;
            for( int k=i; k<argc; k++ ) {
                argv[k] = argv[k+eat];
            }
        }
        else {
            i++;
        }
    }
    if( argc > 1 ) {
        volume_size_x = atoi( argv[1] );
    }
    if( argc > 3 ) {
        volume_size_y = atoi( argv[2] );
        volume_size_z = atoi( argv[3] );
    }
    else {
        volume_size_y = volume_size_x;
        volume_size_z = volume_size_x;
    }
    if( volume_size_x < 4 ) {
        cerr << "Volume size x < 4" << endl;
        exit( EXIT_FAILURE );
    }
    if( volume_size_y < 4 ) {
        cerr << "Volume size y < 4" << endl;
        exit( EXIT_FAILURE );
    }
    if( volume_size_z < 4 ) {
        cerr << "Volume size z < 4" << endl;
        exit( EXIT_FAILURE );
    }

    int device_n = 0;
    cudaGetDeviceCount( &device_n );
    if( device_n == 0 ) {
        std::cerr << "Found no CUDA capable devices." << endl;
        exit( EXIT_FAILURE );
    }
    std::cerr << "Found " << device_n << " CUDA enabled devices:" << endl;
    int best_device = -1;
    int best_device_major = -1;
    int best_device_minor = -1;
    for(int i=0; i<device_n; i++) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties( &prop, i );
        if( (prop.major > best_device_major) || ( (prop.major==best_device_major)&&(prop.minor>best_device_minor) ) ) {
            best_device = i;
            best_device_major = prop.major;
            best_device_minor = prop.minor;
        }
        std::cerr << "    device " << i
                  << ": compute cap=" << prop.major << "." << prop.minor
                  << endl;
    }
    if( device < 0 ) {
        std::cerr << "No CUDA device specified, using device " << best_device << endl;
        device = best_device;
    }
    if( (device < 0) || (device_n <= device) ) {
        std::cerr << "Illegal CUDA device " << device << endl;
        exit( EXIT_FAILURE );
    }
    cudaGLSetGLDevice( device );
//    cudaSetDevice( device );

    // create field

    cudaMalloc( (void**)&field_data_dev, sizeof(unsigned char)*volume_size_x*volume_size_y*volume_size_z );
    bumpyCayley<unsigned char, true, false>
            <<< dim3( (volume_size_x+31)/32, volume_size_z*((volume_size_y+31)/32)), dim3(32,32) >>>
            ( field_data_dev,
              volume_size_x,
              volume_size_y,
              volume_size_z,
              volume_size_x,
              volume_size_x*volume_size_y,
              (volume_size_y+31)/32 );

    std::vector<unsigned char> moo( volume_size_x*volume_size_y*volume_size_z );
    cudaMemcpy( moo.data(), field_data_dev, moo.size(), cudaMemcpyDeviceToHost );

    // Generate OpenGL VBO that we lend to CUDA
    surface_vbo_n = 100;
    glGenBuffers( 1, &surface_vbo );
    glBindBuffer( GL_ARRAY_BUFFER, surface_vbo );
    glBufferData( GL_ARRAY_BUFFER,
                  3*2*3*sizeof(GLfloat)*surface_vbo_n,
                  NULL,
                  GL_DYNAMIC_COPY );
    glGenVertexArrays( 1, &surface_vao );
    glBindVertexArray( surface_vao );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*6, (void*)(3*sizeof(GLfloat)) );
    glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*6, NULL );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray( 0);
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    cudaStreamCreate( &stream );

    // Create profiling events if needed
    if( profile ) {
        cudaEventCreate( &pre_buildup );
        cudaEventCreate( &post_buildup );
        cudaEventCreate( &pre_write );
        cudaEventCreate( &post_write );
    }

    cudaGraphicsGLRegisterBuffer( &surface_resource,
                                  surface_vbo,
                                  cudaGraphicsRegisterFlagsWriteDiscard );


    constants = new cuhpmc::Constants();
    if( gl_direct_draw ) {
        std::vector<unsigned char> foo( volume_size_x*volume_size_y*volume_size_z );
        cudaMemcpy( foo.data(), field_data_dev, foo.size(), cudaMemcpyDeviceToHost );
        cudaFree( field_data_dev );
        field_data_dev = NULL;

        glGenBuffers( 1, &gl_field_buffer );
        glBindBuffer( GL_TEXTURE_BUFFER, gl_field_buffer );
        glBufferData( GL_TEXTURE_BUFFER,
                      foo.size(),
                      foo.data(),
                      GL_STATIC_DRAW );
        glBindBuffer( GL_TEXTURE_BUFFER, 0 );

        field = new cuhpmc::GLFieldUCharBuffer( constants,
                                                gl_field_buffer,
                                                volume_size_x,
                                                volume_size_y,
                                                volume_size_z );

        cuhpmc::GLIsoSurface* srf = new cuhpmc::GLIsoSurface( field );
        iso_surface = srf;
        writer = new cuhpmc::GLWriter( srf );
    }
    else {
        field = new cuhpmc::FieldGlobalMemUChar( constants,
                                                 field_data_dev,
                                                 volume_size_x,
                                                 volume_size_y,
                                                 volume_size_z );
        iso_surface = new cuhpmc::IsoSurface( field );
        writer = new cuhpmc::TriangleVertexWriter( iso_surface );
    }


    cudaError_t error = cudaGetLastError();
    if( error != cudaSuccess ) {
        std::cerr << "CUDA error: " << cudaGetErrorString( error ) << endl;
        exit( EXIT_FAILURE );
    }
}



void
render( float t,
        float dt,
        float fps,
        const GLfloat* P,
        const GLfloat* MV,
        const GLfloat* PM,
        const GLfloat *NM,
        const GLfloat* MV_inv )
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glEnable( GL_DEPTH_TEST );

    iso = 0.5f;//*(sin(t)+1.f);

    // build histopyramid
    if( profile ) {
        cudaEventRecord( pre_buildup, stream );
    }
    iso_surface->build( iso, stream );
    if( profile ) {
        cudaEventRecord( post_buildup, stream );
    }

    // resize buffers if we run unless we do direct GL rendering
    uint triangles = 0;
    if( !gl_direct_draw ) {

        triangles = iso_surface->triangles();
        if( surface_vbo_n < triangles ) {
            if( cudaGraphicsUnregisterResource( surface_resource ) == cudaSuccess ) {
                if( surface_cuda_d != NULL ) {
                    cudaFree( surface_cuda_d );
                    surface_cuda_d = NULL;
                }

                surface_vbo_n = 1.1f*triangles;

                std::vector<GLfloat> foo( 6*3*surface_vbo_n, 0.25f );
                for(size_t i=0; i<3*surface_vbo_n; i++ ) {
                    foo[6*i+3] = 0.5f*(cos( 0.1f*i )+1.f);
                    foo[6*i+4] = 0.5f*(cos( 0.2f*i )+1.f);
                    foo[6*i+5] = 0.5f*(cos( 0.3f*i )+1.f);
                }

                glBindBuffer( GL_ARRAY_BUFFER, surface_vbo );
                glBindBuffer( GL_ARRAY_BUFFER, surface_vbo );
                glBufferData( GL_ARRAY_BUFFER,
                              3*2*3*sizeof(GLfloat)*surface_vbo_n,
                              foo.data(),
                              GL_DYNAMIC_COPY );
                glBindBuffer( GL_ARRAY_BUFFER, surface_vbo );

                cudaGraphicsGLRegisterBuffer( &surface_resource,
                                              surface_vbo,
                                              cudaGraphicsRegisterFlagsNone ) /*,
                                              cudaGraphicsRegisterFlagsWriteDiscard )*/;
            }
            std::cerr << "Resized VBO to hold " << triangles << " triangles (" << (3*2*3*sizeof(GLfloat)*surface_vbo_n) << " bytes)\n";
        }
    }

    if( profile ) {
        cudaEventRecord( pre_write );
    }

    // direct rendering through OpenGL
    if( gl_direct_draw ) {

        if( cuhpmc::GLWriter* w = dynamic_cast<cuhpmc::GLWriter*>( writer ) ) {
            w->render( PM, NM, stream );
        }

    }
    // Let CUDA write, but don't use interop (i.e., no rendering)
    else if( wireframe ) {
        if( cuhpmc::TriangleVertexWriter* w = dynamic_cast<cuhpmc::TriangleVertexWriter*> ( writer ) ) {
            if( surface_cuda_d == NULL ) {
                cudaMalloc( &surface_cuda_d, 3*2*3*sizeof(GLfloat)*surface_vbo_n );
            }
            w->writeInterleavedNormalPosition( surface_cuda_d, triangles, stream );
        }
    }
    // Let CUDA write and let GL render the resulting buffer
    else {
        if( cuhpmc::TriangleVertexWriter* w = dynamic_cast<cuhpmc::TriangleVertexWriter*> ( writer ) ) {
            if( cudaGraphicsMapResources( 1, &surface_resource, stream ) == cudaSuccess ) {
                float* surface_d = NULL;
                size_t surface_size = 0;
                if( cudaGraphicsResourceGetMappedPointer( (void**)&surface_d,
                                                          &surface_size,
                                                          surface_resource ) == cudaSuccess )
                {
                    w->writeInterleavedNormalPosition( surface_d, triangles, stream );
                }
                cudaGraphicsUnmapResources( 1, &surface_resource, stream );
            }
            glMatrixMode( GL_PROJECTION );
            glLoadMatrixf( P );
            glMatrixMode( GL_MODELVIEW );
            glLoadMatrixf( MV );

            glBindVertexArray( surface_vao );
            glDrawArrays( GL_POINTS, 0, 3*triangles );
            glBindVertexArray( 0 );
        }
    }

    if( profile ) {
        cudaEventRecord( post_write );
        cudaEventSynchronize( post_write );

        float ms;
        cudaEventElapsedTime( &ms, pre_buildup, post_buildup );
        buildup_ms += ms;
        cudaEventElapsedTime( &ms, pre_write, post_write );
        write_ms += ms;
        runs++;
    }

    cudaError_t error = cudaGetLastError();
    if( error != cudaSuccess ) {
        std::cerr << "CUDA error: " << cudaGetErrorString( error ) << endl;
        exit( EXIT_FAILURE );
    }
}

const std::string
infoString( float fps )
{
    float avg_buildup = buildup_ms/runs;
    float avg_write = write_ms/runs;
    buildup_ms = 0.f;
    write_ms = 0.f;
    runs = 0;


    std::stringstream o;
    o << std::setprecision(5) << fps << " fps, "
      << volume_size_x << 'x'
      << volume_size_y << 'x'
      << volume_size_z << " samples, "
      << (int)( ((volume_size_x-1)*(volume_size_y-1)*(volume_size_z-1)*fps)/1e6 )
      << " MVPS, ";
    if( profile ) {
        o << "build=" << avg_buildup << "ms, "
          << "write=" << avg_write << "ms, ";
    }
    o << iso_surface->triangles()
      << " triangles, iso=" << iso
      << (wireframe?"[wireframe]":"");
    return o.str();
}
