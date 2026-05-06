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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_POD_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_POD_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/common/types.h>
#include <spa/param/latency.h>
#include <spa/pod/builder.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            struct pod_builder
            {
                private:
                    uint8_t         buffer[1024];
                    spa_pod_builder builder;

                public:
                    pod_builder();
                    pod_builder(const pod_builder &) = delete;
                    pod_builder(pod_builder &&) = delete;
                    ~pod_builder();

                    pod_builder & operator = (const pod_builder &) = delete;
                    pod_builder & operator = (pod_builder &&) = delete;

                public:
                    spa_pod        *make_audio_format_pod(uint32_t sample_rate);
                    spa_pod        *make_midi_format_pod();
                    spa_pod        *make_audio_buffers_pod(size_t max_buffer_size, size_t max_buffers);
                    spa_pod        *make_midi_buffers_pod(size_t max_buffer_size, size_t max_buffers);
                    spa_pod        *make_latency_pod(const spa_latency_info *info);
                    spa_pod        *make_pod_io();
                    spa_pod        *make_pod_async_io();
            };
        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */




#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_POD_H_ */
