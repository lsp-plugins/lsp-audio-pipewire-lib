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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MEMMAP_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MEMMAP_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/pipewire/backend.h>
#include <lsp-plug.in/common/debug.h>

#include <pipewire/data-loop.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            static int memmap_context_loop_free(
                spa_loop *loop, bool async, uint32_t seq,
                const void *data, size_t size, void *self)
            {
                backend_t * const back = cast(self);
                pw_memmap * const * const memmap_ptr = static_cast<pw_memmap * const *>(data);
                lsp_trace("Free memory mapping %p", *memmap_ptr);

                pw_memmap_free(*memmap_ptr);
                pw_core_set_paused(back->pCore, false);

                return 0;
            }

            static int memmap_queue_context_loop_free(
                spa_loop *loop, bool async, uint32_t seq,
                const void *data, size_t size, void *self)
            {
                backend_t * const back = cast(self);
                return pw_loop_invoke(back->pContextLoop, memmap_context_loop_free, 0, data, size, false, back);
            }

            static int memmap_queue_audio_loop_free(backend_t *back, struct pw_memmap *mem)
            {
                if (mem == NULL)
                    return 0;
                if ((back->pCore == NULL) || (back->pContextLoop == NULL))
                {
                    pw_memmap_free(mem);
                    return 0;
                }

                int error = pw_core_set_paused(back->pCore, true);
                if (error >= 0)
                    error = pw_data_loop_invoke(back->pAudioDataLoop,
                        memmap_queue_context_loop_free, SPA_ID_INVALID, &mem, sizeof(&mem), false, back);

                if (error < 0)
                    pw_memmap_free(mem);
                return error;
            }

            template <typename T>
            inline void memmap<T>::construct()
            {
                data            = NULL;
                mapping         = NULL;
            }

            template <typename T>
            inline void memmap<T>::free()
            {
                if (mapping == NULL)
                    return;

                pw_memmap_free(mapping);
                data            = NULL;
                mapping         = NULL;
            }

            template <typename T>
            inline int memmap<T>::unmap(backend_t *backend)
            {
                if (mapping == NULL)
                    return 0;

                pw_memmap * const old = mapping;
                data            = NULL;
                mapping         = NULL;

                return memmap_queue_audio_loop_free(backend, old);
            }

            template <typename T>
            inline int memmap<T>::remap(backend_t *backend, uint32_t mem_id, uint32_t offset, uint32_t size)
            {
                pw_memmap * const old = mapping;
                mapping     = pw_mempool_map_id(backend->pMemPool, mem_id, PW_MEMMAP_FLAG_READWRITE, offset, size, NULL);
                if (mapping == NULL)
                    lsp_warn("Failed to map memory id=%d, offset=%d, size=%d",
                        int(mem_id), int(offset), int(size));

                data        = (mapping != NULL) ? static_cast<T *>(mapping->ptr) : NULL;

                #ifdef LSP_TRACE
                if (data != NULL)
                    lsp_trace("Mapped memory mem_id=%d, offset=%d, size=%d as %p, data ptr=%p",
                        int(mem_id), int(offset), int(size), mapping, data);
            #endif /* LSP_TRACE */

                return memmap_queue_audio_loop_free(backend, old);
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */



#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MEMMAP_H_ */
