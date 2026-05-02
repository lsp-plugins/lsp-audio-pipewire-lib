/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 2 мая 2026 г.
 *
 * lsp-audio-pipewire-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-audio-pipewire-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-audio-pipewire-lib. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/audio/pipewire/port_map.h>

#include <lsp-plug.in/common/alloc.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            port_map::port_map() noexcept
            {
                construct();
            }

            port_map::~port_map() noexcept
            {
                destroy();
            }

            port_map::port_map(port_map && src) noexcept
            {
                construct();
                swap(src);
            }

            port_map & port_map::operator = (port_map && src) noexcept
            {
                swap(src);
                return *this;
            }

            void port_map::construct() noexcept
            {
                vMap            = NULL;
                nFirst          = 0;
                nCapacity       = 0;
            }

            void port_map::destroy() noexcept
            {
                if (vMap == NULL)
                    return;

                free(vMap);
                vMap            = NULL;
                nFirst          = 0;
                nCapacity       = 0;
            }

            port_id_t port_map::map(port_id_t port) noexcept
            {
                // Check port identifier
                if (port < 0)
                    return -STATUS_INVALID_VALUE;

                // Ensure that we have enough space to map
                port_id_t first             = nFirst;
                port_id_t cap               = nCapacity;

                if (first >= cap)
                {
                    const port_id_t new_cap     = lsp_max(cap << 1, port_id_t(4));
                    port_id_t * const new_map   = realloc_count<port_id_t>(vMap, new_cap);
                    if (new_map == NULL)
                        return STATUS_NO_MEM;
                    for (port_id_t i=cap; i<new_cap; ++i)
                        new_map[i]                  = -1;

                    vMap                        = new_map;
                    nCapacity                   = new_cap;
                    cap                         = new_cap;
                }

                // Place new element into the map
                for ( ; first < cap; ++first)
                {
                    if (vMap[first] < 0)
                    {
                        vMap[first]                 = port;
                        nFirst                      = first + 1;
                        return first;
                    }
                }

                nFirst                      = first;
                return -STATUS_CORRUPTED;
            }

            status_t port_map::unmap(port_id_t port) noexcept
            {
                if ((port < 0) || (port >= nCapacity))
                    return STATUS_INVALID_VALUE;
                if (vMap[port] < 0)
                    return STATUS_NOT_FOUND;

                vMap[port]      = -1;
                return STATUS_OK;
            }

            void port_map::swap(port_map & src) noexcept
            {
                lsp::swap(vMap, src.vMap);
                lsp::swap(nFirst, src.nFirst);
                lsp::swap(nCapacity, src.nCapacity);
            }

            void port_map::swap(port_map * src) noexcept
            {
                lsp::swap(vMap, src->vMap);
                lsp::swap(nFirst, src->nFirst);
                lsp::swap(nCapacity, src->nCapacity);
            }
        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


