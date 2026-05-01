/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 1 мая 2026 г.
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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_MEMMAP_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_MEMMAP_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <pipewire/mem.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            struct backend_t;

            /**
             * Typed memory mapping
             * @tparam T type of the memory mapping
             */
            template <typename T>
            struct memmap
            {
                public:
                    T               *data;
                    pw_memmap       *mapping;

                public:
                    /**
                     * Construct an empty object
                     */
                    inline void     construct();

                    /**
                     * Issue direct free
                     */
                    inline void     free();

                    /**
                     * Perform data unmapping
                     * @param backend backend pointer
                     * @return result code
                     */
                    inline int      unmap(backend_t *backend);

                    /**
                     * Perform data re-mapping
                     * @param backend backend pointer
                     * @param mem_id the id of the memory to use
                     * @param offset offset of io area in memory
                     * @param size size of the io area
                     * @return result code
                     */
                    inline int      remap(backend_t *backend, uint32_t mem_id, uint32_t offset, uint32_t size);
            };

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_MEMMAP_H_ */
