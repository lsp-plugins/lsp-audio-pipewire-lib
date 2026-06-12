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

#include <pw-headers/spa/node/io.h>
#include <pw-headers/spa/param/audio/raw.h>
#include <pw-headers/spa/param/format.h>
#include <pw-headers/spa/param/param.h>
#include <pw-headers/spa/param/latency-utils.h>

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

            spa_pod *pod_builder::make_audio_format_pod(uint32_t sample_rate)
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
                spa_pod_builder_add(
                    &builder,
                    SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_audio),
                    SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_dsp),
                    SPA_FORMAT_AUDIO_format, SPA_POD_Id(SPA_AUDIO_FORMAT_DSP_F32),
                    SPA_FORMAT_AUDIO_rate, SPA_POD_Id(sample_rate),
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
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(int32_t(1), int32_t(1), int(max_buffers)),
                    SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(int32_t(1)),
                    SPA_PARAM_BUFFERS_size, SPA_POD_CHOICE_STEP_Int(
                        int32_t(max_buffer_size * sizeof(float)),
                        int32_t(sizeof(float)),
                        int32_t(INT32_MAX),
                        int32_t(sizeof(float))),
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
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(int32_t(1), int32_t(1), int(max_buffers)),
                    SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(int32_t(1)),
                    SPA_PARAM_BUFFERS_size, SPA_POD_CHOICE_STEP_Int(
                        int32_t(max_buffer_size),
                        int32_t(1),
                        int32_t(INT32_MAX),
                        int32_t(1)),
                    SPA_PARAM_BUFFERS_stride, SPA_POD_Int(1),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_latency_pod(const spa_latency_info *info)
            {
                return spa_latency_build(&builder, SPA_PARAM_Latency, info);
            }

            spa_pod *pod_builder::make_process_latency_pod(uint32_t latency, uint32_t sample_rate)
            {
                const float quantum_latency = float(latency) / float(sample_rate);

                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamProcessLatency, SPA_PARAM_ProcessLatency);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_PROCESS_LATENCY_quantum, SPA_POD_Float(float(quantum_latency)),
                    SPA_PARAM_PROCESS_LATENCY_rate, SPA_POD_Int(int32_t(0)),
                    SPA_PARAM_PROCESS_LATENCY_ns, SPA_POD_Long(int64_t(0)),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

            spa_pod *pod_builder::make_pod_io()
            {
                spa_pod_frame frame;
                spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_ParamIO, SPA_PARAM_IO);
                spa_pod_builder_add(
                    &builder,
                    SPA_PARAM_IO_id, SPA_POD_Id(SPA_IO_Buffers),
                    SPA_PARAM_IO_size, SPA_POD_Int(int32_t(sizeof(spa_io_buffers))),
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
                    SPA_PARAM_IO_size, SPA_POD_Int(int32_t(sizeof(spa_io_async_buffers))),
                    0);

                return static_cast<spa_pod *>(spa_pod_builder_pop(&builder, &frame));
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */
