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
#include <cmath>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <builtin_types.h>
#include <vector_functions.h>
#include <cuhpmc/CUDAErrorException.hpp>
#include <cuhpmc/AbstractField.hpp>
#include <cuhpmc/AbstractIsoSurface.hpp>
#include <cuhpmc/AbstractField.hpp>
#include <cuhpmc/FieldGlobalMemUChar.hpp>
#include <cuhpmc/GLFieldUCharBuffer.hpp>
#include <cuhpmc/Constants.hpp>
#include "../kernels/hp5_buildup_base_triple_gb.hpp"
#include "../kernels/hp5_buildup_level_double.hpp"
#include "../kernels/hp5_buildup_level_single.hpp"
#include "../kernels/hp5_buildup_apex.hpp"

namespace cuhpmc {

AbstractIsoSurface::AbstractIsoSurface( AbstractField* field )
    : m_constants( field->constants() ),
      m_field( field )
{
    m_cells = make_uint3( field->width()-1,
                          field->height()-1,
                          field->depth()-1 );

    m_hp5_chunks = make_uint3( (m_cells.x + 30)/31,
                               (m_cells.y + 4)/5,
                               (m_cells.z + 4)/5 );

    // Number of input elements padded s.t. it divides cleanly with blocks
    m_hp5_input_N = 800*m_hp5_chunks.x*m_hp5_chunks.y*m_hp5_chunks.z;

    m_hp5_levels = (uint)ceilf( log2f( (float)m_hp5_input_N )/log2f(5.f) );
    if( m_hp5_levels < 4 ) {
        m_hp5_levels = 4;
    }
    m_hp5_first_single_level = 3;
    if( m_hp5_first_single_level + 3 <= m_hp5_levels ) {
        m_hp5_first_triple_level = m_hp5_levels-3;
    }
    else {
        m_hp5_first_triple_level = m_hp5_levels;
    }
    m_hp5_first_single_level = 3;
    if( m_hp5_first_single_level + 3 <= m_hp5_levels ) {
        m_hp5_first_triple_level = m_hp5_levels-3;
    }
    else {
        m_hp5_first_triple_level = m_hp5_levels;
    }
    m_hp5_first_double_level =
            m_hp5_first_triple_level -
            2*( (m_hp5_first_triple_level - m_hp5_first_single_level)/2 );


    m_hp5_level_sizes.resize( m_hp5_levels );
    m_hp5_offsets.resize( m_hp5_levels );

    uint N = m_hp5_input_N;
    for(uint l=0; l<m_hp5_levels; l++) {
        m_hp5_level_sizes[ m_hp5_levels -1 -l ] = N;
        N = (N+4)/5;
    }

    m_hp5_offsets[0] = 1;
    m_hp5_offsets[1] = 2;
    m_hp5_offsets[2] = 7;
    m_hp5_size = 32;
    for(uint l=m_hp5_first_single_level; l<m_hp5_first_double_level; l++ ) {
        m_hp5_offsets[l] = m_hp5_size;
        m_hp5_size += 5*32*((m_hp5_level_sizes[l]+159)/160);
    }
    for(uint l=m_hp5_first_double_level; l<m_hp5_first_triple_level; l++ ) {
        m_hp5_offsets[l] = m_hp5_size;
        m_hp5_size += 5*32*((m_hp5_level_sizes[l]+799)/800);
    }
    for(uint l=m_hp5_first_triple_level; l<m_hp5_levels; l++ ) {
        m_hp5_offsets[l] = m_hp5_size;
        m_hp5_size += 5*32*((m_hp5_level_sizes[l]+799)/800);
    }

    std::cerr << "    cells = [" << m_cells.x << ", " << m_cells.y << ", " << m_cells.z << "]\n";
    std::cerr << "    chunks = [" << m_hp5_chunks.x << ", " << m_hp5_chunks.y << ", " << m_hp5_chunks.z << "]\n";
    std::cerr << "    unpacked elements = " << m_hp5_input_N << "\n";
    std::cerr << "    levels            = " << m_hp5_levels << ".\n";
    std::cerr << "    levels layout:\n";
    for( uint i=0; i<m_hp5_levels; i++ ) {
        std::cerr << "        L" << i <<", kernel=";
        if( i < m_hp5_first_single_level ) {
            std::cerr << "apex";
        }
        else if( i < m_hp5_first_double_level ) {
            std::cerr << "single";
        }
        else if( i < m_hp5_first_triple_level ) {
            std::cerr << "double";
        }
        else {
            std::cerr << "triple";
        }
        std::cerr << ", size=" << m_hp5_level_sizes[i];
        std::cerr << ", offset=" << m_hp5_offsets[i] << "\n";
    }

    cudaError_t error;

    // --- set up sideband memory
    error = cudaMalloc( (void**)&m_hp5_sb_d, sizeof(uint)*m_hp5_size );
    if( error != cudaSuccess ) {
        throw CUDAErrorException( error );
    }

    // --- set up mem and event for zero-copy of top element
    error = cudaHostAlloc( (void**)&m_hp5_top_h, 2*sizeof(uint), cudaHostAllocMapped );
    if( error != cudaSuccess ) {
        throw CUDAErrorException( error );
    }
    m_hp5_top_h[0] = 0; // triangles
    m_hp5_top_h[1] = 0; // vertices

    error = cudaHostGetDevicePointer( (void**)&m_hp5_top_d, m_hp5_top_h, 0 );
    if( error != cudaSuccess ) {
        throw CUDAErrorException( error );
    }
    error = cudaEventCreateWithFlags( &m_buildup_event, cudaEventDisableTiming );
    if( error != cudaSuccess ) {
        throw CUDAErrorException( error );
    }
}

AbstractIsoSurface::~AbstractIsoSurface( )
{
    cudaError_t error;
    if( m_hp5_top_h != NULL ) {
        error = cudaFreeHost( m_hp5_top_h );
        m_hp5_top_h = NULL;
        if( error != cudaSuccess ) {
            throw CUDAErrorException( error );
        }

    }
}

uint
AbstractIsoSurface::triangles()
{
    cudaEventSynchronize( m_buildup_event );
    return m_hp5_top_h[0]/3;
}

void
AbstractIsoSurface::buildNonIndexed( float iso, uint4* hp5_hp_d, unsigned char* case_d, cudaStream_t stream )
{

    m_iso = iso;
    uint3 field_size = make_uint3( m_field->width(), m_field->height(), m_field->depth() );

    if( FieldGlobalMemUChar* field = dynamic_cast<FieldGlobalMemUChar*>( m_field ) ) {
        run_hp5_buildup_base_triple_gb_ub( hp5_hp_d + m_hp5_offsets[ m_hp5_levels-3 ],
                                           m_hp5_sb_d + m_hp5_offsets[ m_hp5_levels-3 ],
                                           m_hp5_level_sizes[ m_hp5_levels-1 ],
                                           hp5_hp_d + m_hp5_offsets[ m_hp5_levels-2 ],
                                           hp5_hp_d + m_hp5_offsets[ m_hp5_levels-1 ],
                                           case_d,
                                           m_iso,
                                           m_hp5_chunks,
                                           field->fieldDev(),
                                           field_size,
                                           m_constants->triangleIndexCountDev(),
                                           stream );


    }
    else if( GLFieldUCharBuffer* field = dynamic_cast<GLFieldUCharBuffer*>( m_field ) ) {
        const unsigned char* field_d = field->mapFieldBuffer( stream );
        run_hp5_buildup_base_triple_gb_ub( hp5_hp_d + m_hp5_offsets[ m_hp5_levels-3 ],
                                           m_hp5_sb_d + m_hp5_offsets[ m_hp5_levels-3 ],
                                           m_hp5_level_sizes[ m_hp5_levels-1 ],
                                           hp5_hp_d + m_hp5_offsets[ m_hp5_levels-2 ],
                                           hp5_hp_d + m_hp5_offsets[ m_hp5_levels-1 ],
                                           case_d,
                                           m_iso,
                                           m_hp5_chunks,
                                           field_d,
                                           field_size,
                                           m_constants->triangleIndexCountDev(),
                                           stream );
        field->unmapFieldBuffer( stream );
    }
    else {
        throw std::runtime_error( "Unsupported field type" );
    }
    for( uint i=m_hp5_first_triple_level; i>m_hp5_first_double_level; i-=2 ) {
        run_hp5_buildup_level_double( hp5_hp_d + m_hp5_offsets[i-2],
                                      m_hp5_sb_d + m_hp5_offsets[i-2],
                                      hp5_hp_d + m_hp5_offsets[i-1],
                                      m_hp5_sb_d + m_hp5_offsets[i],
                                      m_hp5_level_sizes[i-1],
                                      stream );
    }
    for( uint i=m_hp5_first_double_level; i>m_hp5_first_single_level; --i ) {
        run_hp5_buildup_level_single( hp5_hp_d + m_hp5_offsets[ i-1 ],
                                      m_hp5_sb_d + m_hp5_offsets[ i-1 ],
                                      m_hp5_sb_d + m_hp5_offsets[ i   ],
                                      m_hp5_level_sizes[i-1],
                                      stream );
    }
    run_hp5_buildup_apex( m_hp5_top_d,
                          hp5_hp_d,
                          m_hp5_sb_d + 32,
                          m_hp5_level_sizes[2],
                          stream );
    cudaEventRecord( m_buildup_event, stream );
}



} // of namespace cuhpmc
