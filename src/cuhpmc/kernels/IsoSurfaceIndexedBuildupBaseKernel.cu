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

#include <assert.h>
#include <iostream>
#include <stdexcept>
#include <cuhpmc/Constants.hpp>
#include <cuhpmc/FieldGlobalMemUChar.hpp>
#include <cuhpmc/IsoSurface.hpp>
#include <cuhpmc/IsoSurfaceIndexed.hpp>
#include <cuhpmc/CUDAErrorException.hpp>

namespace cuhpmc {

template<class T>
__device__
void
fetchFromField( uint&           bp0,                // Bit mask for slice 0
                uint&           bp1,                // Bit mask for slice 1
                uint&           bp2,                // Bit mask for slice 2
                uint&           bp3,                // Bit mask for slice 3
                uint&           bp4,                // Bit mask for slice 4
                uint&           bp5,                // Bit mask for slice 5
                const T*        field,              // Sample-adjusted field pointer
                const T*        field_end,          // Pointer to buffer end
                const size_t    field_row_pitch,
                const size_t    field_slice_pitch,
                const float     iso,
                const bool      no_check )
{
    const T* llfield = field;
    if( no_check ) {
        bp0 = (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp1 = (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp2 = (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp3 = (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp4 = (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp5 = (*llfield < iso) ? 1 : 0;
    }
    else {
        bp0 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp1 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp2 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp3 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp4 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
        llfield += field_slice_pitch;
        bp5 = ( llfield < field_end ) && (*llfield < iso) ? 1 : 0;
    }
}


template<class T>
struct hp5_buildup_base_indexed_triple_gb_args
{
    uint4* __restrict__                 tri_pyramid_level_a_d;
    uint4* __restrict__                 vtx_pyramid_level_a_d;
    uint4* __restrict__                 tri_pyramid_level_b_d;
    uint4* __restrict__                 vtx_pyramid_level_b_d;
    uint4* __restrict__                 tri_pyramid_level_c_d;
    uint4* __restrict__                 vtx_pyramid_level_c_d;
    uint*  __restrict__                 tri_sideband_level_c_d;
    uint*  __restrict__                 vtx_sideband_level_c_d;
    unsigned char* __restrict__         d_case;
    float                               iso;
    int3                                cells;
    int3                                chunks;
    const T* __restrict__               field;
    int                                 field_row_pitch;
    int                                 field_slice_pitch;
    const unsigned char* __restrict__   case_vtxtricnt;
};

template<class T>
__global__
void
__launch_bounds__( 160 )
hp5_buildup_base_indexed_triple_gb( hp5_buildup_base_indexed_triple_gb_args<T> a )
{
    __shared__ uint sb[800];
    __shared__ uint sh[801];

    const int w  = threadIdx.x / 32;                                   // warp
    const int wt = threadIdx.x % 32;                                   // thread-in-warp
    const int sh_i = 160*w + 5*wt;                                     //
    const int hp_b_o = 5*32*blockIdx.x + 32*w + wt;                    //
    const int c_lix = 5*blockIdx.x + w;                                //

    const int3 chunk = make_int3( c_lix % a.chunks.x,
                                  (c_lix/a.chunks.x) % a.chunks.y,
                                  (c_lix/a.chunks.x) / a.chunks.y );
    if( chunk.z < a.chunks.z ) {

        // Base xyz-index for this chunk. Can be composed into a single uint32, and
        // be checked with an LOP.AND + ISETP.LT
        const int3 chunk_offset = make_int3( 31*chunk.x,
                                             5*chunk.y,
                                             5*chunk.z );

        assert( chunk_offset.x <= a.cells.x );
        assert( chunk_offset.y <= a.cells.y );
        //assert( chunk_offset.z <= a.cells.z );

        const int3 chunk_cells = make_int3( a.cells.x - chunk_offset.x,
                                            a.cells.y - chunk_offset.y,
                                            a.cells.z - chunk_offset.z );

        // base corner should always be inside field, but x for wt > 0 might be
        // outside.
        int field_offset = min( a.cells.z, chunk_offset.z) * a.field_slice_pitch
                + chunk_offset.y * a.field_row_pitch
                + chunk_offset.x + min( wt, chunk_cells.x );


        // Fetch scalar field values and determine inside-outside for 5 slices
        int field_offset_tmp = field_offset;
        uint bp0 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
        if( 1 <= chunk_cells.z ) {
            field_offset_tmp += a.field_slice_pitch;
        }
        uint bp1 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
        if( 2 <= chunk_cells.z ) {
            field_offset_tmp += a.field_slice_pitch;
        }
        uint bp2 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
        if( 3 <= chunk_cells.z ) {
            field_offset_tmp += a.field_slice_pitch;
        }
        uint bp3 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
        if( 4 <= chunk_cells.z ) {
            field_offset_tmp += a.field_slice_pitch;
        }
        uint bp4 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
        if( 5 <= chunk_cells.z ) {
            field_offset_tmp += a.field_slice_pitch;
        }
        uint bp5 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;

        for(uint q=0; q<5; q++) {
            // Move along y to build up masks
            if( q+1 <= chunk_cells.y ) {
                field_offset += a.field_row_pitch;
            }
            int field_offset_tmp = field_offset;
            uint bc0 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;

            if( 1 <= chunk_cells.z ) {
                field_offset_tmp += a.field_slice_pitch;
            }
            uint bc1 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
            if( 2 <= chunk_cells.z ) {
                field_offset_tmp += a.field_slice_pitch;
            }
            uint bc2 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
            if( 3 <= chunk_cells.z ) {
                field_offset_tmp += a.field_slice_pitch;
            }
            uint bc3 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
            if( 4 <= chunk_cells.z ) {
                field_offset_tmp += a.field_slice_pitch;
            }
            uint bc4 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;
            if( 5 <= chunk_cells.z ) {
                field_offset_tmp += a.field_slice_pitch;
            }
            uint bc5 = a.field[ field_offset_tmp ] < a.iso ? 1 : 0;

            // Merge
            uint b0 = bp0 + (bc0<<2);
            uint b1 = bp1 + (bc1<<2);
            uint b2 = bp2 + (bc2<<2);
            uint b3 = bp3 + (bc3<<2);
            uint b4 = bp4 + (bc4<<2);
            uint b5 = bp5 + (bc5<<2);
            // Store for next iteration
            bp0 = bc0;
            bp1 = bc1;
            bp2 = bc2;
            bp3 = bc3;
            bp4 = bc4;
            bp5 = bc5;

            // build case
            uint m0_1 = b0 + (b1<<4);
            uint m1_1 = b1 + (b2<<4);
            uint m2_1 = b2 + (b3<<4);
            uint m3_1 = b3 + (b4<<4);
            uint m4_1 = b4 + (b5<<4);
            sh[ 0*160 + threadIdx.x ] = m0_1;
            sh[ 1*160 + threadIdx.x ] = m1_1;
            sh[ 2*160 + threadIdx.x ] = m2_1;
            sh[ 3*160 + threadIdx.x ] = m3_1;
            sh[ 4*160 + threadIdx.x ] = m4_1;

            uint ix_o_1 = 160*w + 32*q + wt;

            uint mask;
            if(  (wt <= 30) &&
                 (wt <= chunk_cells.x ) &&
                 (q <= chunk_cells.y ) )
            {
                if( (wt == chunk_cells.x) || (q==chunk_cells.y) ) {
                    mask = 0xf0u;
                }
                else {
                    mask = ~0x0u;
                }
            }
            else {
                mask = 0x0u;
            }

            // merge from right
            //if( xmask_tri && ymask_tri && wt < 31 ) {
            uint sum;
            if( mask != 0u ) {
                m0_1 += (sh[ 0*160 + threadIdx.x + 1]<<1);
                m1_1 += (sh[ 1*160 + threadIdx.x + 1]<<1);
                m2_1 += (sh[ 2*160 + threadIdx.x + 1]<<1);
                m3_1 += (sh[ 3*160 + threadIdx.x + 1]<<1);
                m4_1 += (sh[ 4*160 + threadIdx.x + 1]<<1);

                // cnt_a_X = %00000000 0vv00ttt


                uint cnt_a_0;
                uint cnt_a_1;
                uint cnt_a_2;
                uint cnt_a_3;
                uint cnt_a_4;
                if( mask == ~0x0u && 5 < chunk_cells.z ) {
                    cnt_a_0 = a.case_vtxtricnt[ m0_1 ];
                    cnt_a_1 = a.case_vtxtricnt[ m1_1 ];
                    cnt_a_2 = a.case_vtxtricnt[ m2_1 ];
                    cnt_a_3 = a.case_vtxtricnt[ m3_1 ];
                    cnt_a_4 = a.case_vtxtricnt[ m4_1 ];
                }
                else {
                    uint tmp_mask;
                    if( 0 <= chunk_cells.z ) {
                        tmp_mask = mask;
                    }
                    else {
                        tmp_mask = 0x00u;
                    }
                    if( 0 == chunk_cells.z ) {
                        tmp_mask = tmp_mask & 0xf0u;
                        mask = 0x00u;
                    }
                    cnt_a_0 = a.case_vtxtricnt[ m0_1 ] & tmp_mask;

                    tmp_mask = mask;
                    if( 1 == chunk_cells.z ) {
                        tmp_mask = tmp_mask & 0xf0u;
                        mask = 0x00u;
                    }
                    cnt_a_1 = a.case_vtxtricnt[ m1_1 ] & tmp_mask;

                    tmp_mask = mask;
                    if( 2 == chunk_cells.z ) {
                        tmp_mask = tmp_mask & 0xf0u;
                        mask = 0x00u;
                    }
                    cnt_a_2 = a.case_vtxtricnt[ m2_1 ] & tmp_mask;

                    tmp_mask = mask;
                    if( 3 == chunk_cells.z ) {
                        tmp_mask = tmp_mask & 0xf0u;
                        mask = 0x00u;
                    }
                    cnt_a_3 = a.case_vtxtricnt[ m3_1 ] & tmp_mask;

                    tmp_mask = mask;
                    if( 4 == chunk_cells.z ) {
                        tmp_mask = tmp_mask & 0xf0u;
                    }
                    cnt_a_4 = a.case_vtxtricnt[ m4_1 ] & tmp_mask;
                }


                sum = cnt_a_0
                        + cnt_a_1
                        + cnt_a_2
                        + cnt_a_3
                        + cnt_a_4;

                // sum = %00000000 00000000 0000000v vvvttttt
                // sb  = %00000000 0000vvvv 00000000 000ttttt
                sb[ ix_o_1 ] = ((sum<<11)&0xf0000u) | (sum&0x1fu);
                if( sum > 0 ) {
                    // triangle count stored as 4 x 4 bits = 16 bits
                    ((short1*)(a.tri_pyramid_level_a_d))[ 5*160*blockIdx.x + ix_o_1 ] =
                            make_short1( ((cnt_a_0 & 0xf)) |
                                         ((cnt_a_1 & 0xf)<<4) |
                                         ((cnt_a_2 & 0xf)<<8) |
                                         ((cnt_a_3 & 0xf)<<12) );

                    // vertex count stored as 4 x 2 bits = 8 bits
                    ((unsigned char*)(a.vtx_pyramid_level_a_d))[ 5*160*blockIdx.x + ix_o_1 ]
                            = ((cnt_a_0>>5u)
                               |  (cnt_a_1>>3u)
                               |  ((cnt_a_2>>1u)&0x30u)
                               |  ((cnt_a_3<<1u)&0xc0u) ) & 0xffu;


                    //   a.tri_pyramid_level_a_d[ 5*160*blockIdx.x + ix_o_1 ] = make_uint4( s0_1, s1_1, s2_1, s3_1 );
                    a.d_case[ 5*(5*160*blockIdx.x + 160*w + 32*q + wt) + 0 ] = m0_1;
                    a.d_case[ 5*(5*160*blockIdx.x + 160*w + 32*q + wt) + 1 ] = m1_1;
                    a.d_case[ 5*(5*160*blockIdx.x + 160*w + 32*q + wt) + 2 ] = m2_1;
                    a.d_case[ 5*(5*160*blockIdx.x + 160*w + 32*q + wt) + 3 ] = m3_1;
                    a.d_case[ 5*(5*160*blockIdx.x + 160*w + 32*q + wt) + 4 ] = m4_1;
                }
            }
            else {
                sb[ ix_o_1 ] = 0;
            }
        }
    }
    else {
        // pad-chunk, just write zero's and we'll never see it again.
        for(uint q=0; q<5; q++) {
            uint ix_o_1 = 160*w + 32*q + wt;
            sb[ ix_o_1 ] = 0;
        }
    }
    // second reduction
    uint cnt_b_0 = sb[ sh_i + 0 ];
    uint cnt_b_1 = sb[ sh_i + 1 ];
    uint cnt_b_2 = sb[ sh_i + 2 ];
    uint cnt_b_3 = sb[ sh_i + 3 ];
    uint cnt_b_4 = sb[ sh_i + 4 ];

    // triangle count as 4 x 8 bits = 32 bits
    ((uchar4*)a.tri_pyramid_level_b_d)[ hp_b_o ] = make_uchar4( cnt_b_0,
                                                                cnt_b_1,
                                                                cnt_b_2,
                                                                cnt_b_3 );

    // vertex count stored as 4 x 4 bits = 16 bits
    ((short1*)a.vtx_pyramid_level_b_d)[ hp_b_o ] = make_short1( (cnt_b_0>>16u) |
                                                                (cnt_b_1>>12u) |
                                                                (cnt_b_2>>8u)  |
                                                                (cnt_b_3>>4u) );


    __syncthreads();
    // third reduction
    // sh = %00000000 0vvvvvvv 00000000 0ttttttt
    sh[ 32*w + wt ] = cnt_b_0
                    + cnt_b_1
                    + cnt_b_2
                    + cnt_b_3
                    + cnt_b_4;
    __syncthreads();
    if( w == 0 ) {
        uint cnt_c_0 = sh[5*wt+0];
        uint cnt_c_1 = sh[5*wt+1];
        uint cnt_c_2 = sh[5*wt+2];
        uint cnt_c_3 = sh[5*wt+3];
        uint cnt_c_4 = sh[5*wt+4];

        // triangle count stored as 4 x 8 bits
        ((uchar4*)a.tri_pyramid_level_c_d)[ 32*blockIdx.x + wt ] = make_uchar4( cnt_c_0,
                                                                                cnt_c_1,
                                                                                cnt_c_2,
                                                                                cnt_c_3 );
        // vertex count stored as 4 x 8 bits
        ((uchar4*)a.vtx_pyramid_level_c_d)[ 32*blockIdx.x + wt ] = make_uchar4( (cnt_c_0>>16),
                                                                                (cnt_c_1>>16),
                                                                                (cnt_c_2>>16),
                                                                                (cnt_c_3>>16) );

        // sum = %0000000v vvvvvvvv 000000tt tttttttt
        uint sum = cnt_c_0
                 + cnt_c_1
                 + cnt_c_2
                 + cnt_c_3
                 + cnt_c_4;

        a.tri_sideband_level_c_d[ 32*blockIdx.x + wt ] = sum       & 0xffffu;
        a.vtx_sideband_level_c_d[ 32*blockIdx.x + wt ] = (sum>>16) & 0xffffu;
    }
}

void
IsoSurfaceIndexed::invokeBaseBuildup( cudaStream_t stream )
{
    if( FieldGlobalMemUChar* field = dynamic_cast<FieldGlobalMemUChar*>( m_field ) ) {

        hp5_buildup_base_indexed_triple_gb_args<unsigned char> args;
        args.tri_pyramid_level_a_d  = m_triangle_pyramid_d + m_hp5_offsets[ m_hp5_levels-1 ];
        args.vtx_pyramid_level_a_d  = m_vertex_pyramid_d   + m_hp5_offsets[ m_hp5_levels-1 ];
        args.tri_pyramid_level_b_d  = m_triangle_pyramid_d + m_hp5_offsets[ m_hp5_levels-2 ];
        args.vtx_pyramid_level_b_d  = m_vertex_pyramid_d   + m_hp5_offsets[ m_hp5_levels-2 ];
        args.tri_pyramid_level_c_d  = m_triangle_pyramid_d + m_hp5_offsets[ m_hp5_levels-3 ];
        args.vtx_pyramid_level_c_d  = m_vertex_pyramid_d   + m_hp5_offsets[ m_hp5_levels-3 ];
        args.tri_sideband_level_c_d = m_triangle_sideband_d + m_hp5_offsets[ m_hp5_levels-3 ];
        args.vtx_sideband_level_c_d = m_vertex_sideband_d + m_hp5_offsets[ m_hp5_levels-3 ];
        args.d_case             = m_case_d;
        args.iso                = 256.f*m_iso;
        args.cells              = make_int3( field->width()-1,
                                             field->height()-1,
                                             field->depth()-1 );
        args.chunks             = make_int3( m_hp5_chunks.x,
                                             m_hp5_chunks.y,
                                             m_hp5_chunks.z );
        args.field              = field->fieldDev();
        args.field_row_pitch    = field->width();
        args.field_slice_pitch  = field->width()*field->height();
        args.case_vtxtricnt     = m_constants->vertexTriangleCountDev() ;

        uint gs = (m_hp5_level_sizes[ m_hp5_levels-1 ]+3999)/4000;
        uint bs = 160;
        hp5_buildup_base_indexed_triple_gb<unsigned char><<<gs,bs,0, stream >>>( args );

    }
    else {
        throw std::runtime_error( "invokeBaseBuildup: unsupported field type" );
    }
    cudaError_t error = cudaGetLastError();
    if( error != cudaSuccess ) {
        throw CUDAErrorException( error );
    }
}


} // of namespace cuhpmc
