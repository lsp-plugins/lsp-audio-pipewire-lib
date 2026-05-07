/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 7 мая 2026 г.
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

#include <lsp-plug.in/audio/pipewire/ringbuffer.h>
#include <lsp-plug.in/common/alloc.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            ringbuffer::ringbuffer() noexcept
            {
                construct();
            }

            ringbuffer::~ringbuffer() noexcept
            {
                destroy();
            }

            void ringbuffer::construct() noexcept
            {
                pEvent          = NULL;
                vBuffer         = NULL;
                nSize           = 0;
                spa_ringbuffer_init(&sRing);
            }

            void ringbuffer::destroy() noexcept
            {
                if (vBuffer != NULL)
                {
                    free(vBuffer);
                    vBuffer         = NULL;
                }

                pEvent          = NULL;
                nSize           = 0;
            }

            status_t ringbuffer::init(
                pw_loop *loop, uint32_t size,
                spa_source_event_func_t handler, void *data) noexcept
            {
                uint8_t * buffer  = malloc_count<uint8_t>(size);
                if (buffer == NULL)
                    return STATUS_NO_MEM;
                lsp_finally {
                    if (buffer != NULL)
                        free(buffer);
                };

                spa_ringbuffer_init(&sRing);
                spa_source * const event    = pw_loop_add_event(loop, handler, data);
                if (event == NULL)
                    return STATUS_UNKNOWN_ERR;

                pLoop           = loop;
                pEvent          = event;
                vBuffer         = release_ptr(buffer);
                nSize           = size;

                return STATUS_OK;
            }

            ssize_t ringbuffer::read(void *dst, uint32_t count) noexcept
            {
                // Check for overrun and underrun
                uint32_t read_index = 0;
                const int32_t avail = spa_ringbuffer_get_read_index(&sRing, &read_index);
                if (avail < 0)
                {
                    spa_ringbuffer_read_update(&sRing, read_index + avail);
                    return -STATUS_UNDERFLOW;
                }
                else if (uint32_t(avail) > nSize)
                {
                    spa_ringbuffer_read_update(&sRing, read_index + avail);
                    return -STATUS_OVERFLOW;
                }
                else if (uint32_t(avail) < count)
                    return (avail == 0) ? -STATUS_NO_DATA : -STATUS_CORRUPTED;

                // Perform data read
                spa_ringbuffer_read_data(
                    &sRing, vBuffer, nSize,
                    read_index % nSize, dst, count);
                spa_ringbuffer_read_update(&sRing, read_index + count);

                return count;
            }

            status_t ringbuffer::write(const void *src, uint32_t count) noexcept
            {
                // Ensure that we can put the whole message to buffer
                if (count > nSize)
                    return STATUS_TOO_BIG;

                // Ensure that buffer is ready to receive the data
                uint32_t write_index    = 0;
                const int32_t avail     = int32_t(nSize) - spa_ringbuffer_get_write_index(&sRing, &write_index);
                if (avail < int32_t(count))
                    return STATUS_RETRY;

                // Write data to buffer
                spa_ringbuffer_write_data(
                    &sRing, vBuffer, nSize,
                    write_index % nSize, src, count);
                spa_ringbuffer_write_update(&sRing, write_index + count);

                // Signal event ot the loop
                pw_loop_signal_event(pLoop, pEvent);
                return STATUS_OK;
            }

        }  /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


