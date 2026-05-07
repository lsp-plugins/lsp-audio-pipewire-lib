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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_RINGBUFFER_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_RINGBUFFER_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/common/types.h>

#include <pipewire/loop.h>
#include <spa/support/loop.h>
#include <spa/utils/ringbuffer.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            /**
             * Ring buffer for data exchange between RT and Context threads.
             */
            typedef struct ringbuffer
            {
                protected:
                    pw_loop            *pLoop;
                    spa_source         *pEvent;
                    uint8_t            *vBuffer;
                    spa_ringbuffer      sRing;
                    uint32_t            nSize;

                public:
                    ringbuffer() noexcept;
                    ringbuffer(const ringbuffer &) = delete;
                    ringbuffer(ringbuffer &&) = delete;
                    ~ringbuffer() noexcept;

                    ringbuffer & operator = (const ringbuffer &) = delete;
                    ringbuffer & operator = (ringbuffer &&) = delete;

                    void                construct() noexcept;
                    void                destroy() noexcept;

                public:
                    status_t            init(
                        pw_loop * loop, uint32_t size,
                        spa_source_event_func_t handler, void *data) noexcept;

                    ssize_t             read(void *dst, uint32_t count) noexcept;
                    status_t            write(const void *src, uint32_t count) noexcept;

            } ringbuffer;
        }  /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_RINGBUFFER_H_ */
