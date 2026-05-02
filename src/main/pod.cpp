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

#include <lsp-plug.in/audio/pipewire/pod.h>

#include <lsp-plug.in/audio/pipewire/impl/pw-defs.h>

#include <spa/node/io.h>
#include <spa/param/audio/raw.h>
#include <spa/param/format.h>
#include <spa/param/param.h>
#include <spa/param/latency-utils.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            pod_builder::pod_builder()
            {
                builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
            }

            pod_builder::~pod_builder()
            {
            }

            spa_pod *pod_builder::make_audio_format_pod()
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
                spa_pod_builder_add(
                    &builder,
                    SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_audio),
                    SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_dsp),
                    SPA_FORMAT_AUDIO_format, SPA_POD_Id(SPA_AUDIO_FORMAT_DSP_F32),
                    0);
                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_midi_format_pod()
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
                spa_pod_builder_add(
                    &builder,
                    SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_application),
                    SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_control),
                    0);
                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_audio_buffers_pod(size_t max_buffer_size, size_t max_buffers)
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(1, 1, int(max_buffers)),
                    SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                    SPA_PARAM_BUFFERS_size, SPA_POD_CHOICE_STEP_Int(
                        int(max_buffer_size * sizeof(float)),
                        sizeof(float), INT32_MAX, sizeof(float)),
                    SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(sizeof(float)),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_midi_buffers_pod(size_t max_buffer_size, size_t max_buffers)
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(1, 1, int(max_buffers)),
                    SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                    SPA_PARAM_BUFFERS_size, SPA_POD_CHOICE_STEP_Int(
                        int(max_buffer_size),
                        1, INT32_MAX, 1),
                    SPA_PARAM_BUFFERS_stride, SPA_POD_Int(1),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_latency_pod(const spa_latency_info *info)
            {
                return spa_latency_build(&builder, SPA_PARAM_Latency, info);
            }

            spa_pod *pod_builder::make_pod_io()
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamIO, SPA_PARAM_IO);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_IO_id, SPA_POD_Id(SPA_IO_Buffers),
                    SPA_PARAM_IO_size, SPA_POD_Int(sizeof(spa_io_buffers)),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_pod_async_io()
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamIO, SPA_PARAM_IO);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_IO_id, SPA_POD_Id(SPA_IO_AsyncBuffers),
                    SPA_PARAM_IO_size, SPA_POD_Int(sizeof(spa_io_async_buffers)),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */
