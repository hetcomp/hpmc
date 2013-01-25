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
#include <cuhpmc/cuhpmc.hpp>
#include <cuhpmc/CUDAErrorException.hpp>
#include <cuhpmc/FieldGlobalMemUChar.hpp>

namespace cuhpmc {

FieldGlobalMemUChar::FieldGlobalMemUChar( Constants*     constants,
                                          unsigned char* field_dev,
                                          uint           width,
                                          uint           height,
                                          uint           depth )
    : AbstractField( constants, width, height, depth ),
      m_field_dev( field_dev )
{

}

FieldGlobalMemUChar::~FieldGlobalMemUChar()
{
}

} // of namespace cuhpmc
