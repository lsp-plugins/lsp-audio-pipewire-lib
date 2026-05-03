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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_PORT_MAP_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_PORT_MAP_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/iface/types.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/common/types.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            /**
             * Port mapping: map port identifier to some non-negative value which tends to zero.
             */
            struct port_map
            {
                private:
                    port_id_t      *vMap;
                    port_id_t       nFirst;
                    port_id_t       nCapacity;

                public:
                    port_map() noexcept;
                    port_map(const port_map &) = delete;
                    port_map(port_map && src) noexcept;
                    ~port_map() noexcept;

                    port_map & operator = (const port_map &) = delete;
                    port_map & operator = (port_map && src) noexcept;

                    void            construct() noexcept;
                    void            destroy() noexcept;

                public:
                    /**
                     * Map port
                     * @param port identifier to map
                     * @return mapped port identifier or negative error code
                     */
                    port_id_t       map(port_id_t port) noexcept;

                    /**
                     * Unmap port
                     * @param port identifier to unmap
                     * @return status of operation
                     */
                    status_t        unmap(port_id_t port) noexcept;

                    void            swap(port_map & src) noexcept;
                    void            swap(port_map * src) noexcept;
            };
        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */



#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_PORT_MAP_H_ */
