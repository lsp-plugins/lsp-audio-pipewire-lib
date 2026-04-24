/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 6 апр. 2026 г.
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

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/common/status.h>

#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/audio/pipewire/backend.h>

#include <jack/midiport.h>

#include <stdlib.h>
#include <errno.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            static constexpr uint32_t PORT_TYPE_FREE        = 0xffffffff;
            static constexpr uint32_t PORT_MASK_ALL         = PORT_DIR_MASK | PORT_TYPE_MASK;

            static inline pipewire::backend_t *cast(audio::backend_t *self)
            {
                return static_cast<pipewire::backend_t *>(self);
            }

            static inline pipewire::backend_t *cast(void *self)
            {
                return static_cast<pipewire::backend_t *>(self);
            }

            backend_t::backend_t()
            {
                construct();
            }

            void backend_t::construct()
            {
                pUserData                       = NULL;
                pCallbacks                      = NULL;

                io_parameters_t * const ip      = &sIOParams;
                ip->sample_rate                 = 0;
                ip->buffer_size                 = 0;
                ip->max_buffer_size             = 0;

                io_position_t * const npos      = &sIOPosition;
                npos->frame                     = 0;
                npos->bar                       = 0;
                npos->beat                      = 0;
                npos->tick                      = 0;
                npos->speed                     = 1.0f;
                npos->numerator                 = 4.0f;
                npos->denominator               = 4.0f;
                npos->beats_per_minute          = 120.0f;
                npos->beats_per_minute_change   = 0.0f;
                npos->ticks_per_beat            = 4096.0f;

                nLatency                        = 0;

                // Export virtual table
                #define AUDIO_PIPEWIRE_BACKEND_EXP(func)    audio::backend_t::func = backend_t::func;

                AUDIO_PIPEWIRE_BACKEND_EXP(connect);
                AUDIO_PIPEWIRE_BACKEND_EXP(set_latency);
                AUDIO_PIPEWIRE_BACKEND_EXP(disconnect);
                AUDIO_PIPEWIRE_BACKEND_EXP(destroy);

                AUDIO_PIPEWIRE_BACKEND_EXP(register_port);
                AUDIO_PIPEWIRE_BACKEND_EXP(unregister_port);
                AUDIO_PIPEWIRE_BACKEND_EXP(set_port_latency);
                AUDIO_PIPEWIRE_BACKEND_EXP(port_system_name);

                AUDIO_PIPEWIRE_BACKEND_EXP(connect_ports);
                AUDIO_PIPEWIRE_BACKEND_EXP(disconnect_ports);

                AUDIO_PIPEWIRE_BACKEND_EXP(audio_buffers_count);
                AUDIO_PIPEWIRE_BACKEND_EXP(get_audio_buffer);

                AUDIO_PIPEWIRE_BACKEND_EXP(midi_events_count);
                AUDIO_PIPEWIRE_BACKEND_EXP(read_midi_event);
                AUDIO_PIPEWIRE_BACKEND_EXP(write_midi_event);

                #undef AUDIO_PIPEWIRE_BACKEND_EXP
            }

            status_t backend_t::connect(
                audio::backend_t *self,
                const connection_params_t *params,
                const callbacks_t *callbacks,
                void *user_data)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            status_t backend_t::set_latency(audio::backend_t *self, uint32_t latency)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            status_t backend_t::disconnect(audio::backend_t *self)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            void backend_t::destroy(audio::backend_t *self)
            {
                backend_t * const back          = cast(self);

                // Issue disconnect and free allocated memory
                disconnect(back);

                // Deallocate memory
                free(back);
            }

            port_id_t backend_t::register_port(audio::backend_t *self, const char *id, uint32_t flags)
            {
                return -STATUS_NOT_IMPLEMENTED;
            }

            status_t backend_t::unregister_port(audio::backend_t *self, port_id_t port_id)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            const char *backend_t::port_system_name(audio::backend_t *self, port_id_t port_id)
            {
                return NULL;
            }

            status_t backend_t::set_port_latency(audio::backend_t *self, port_id_t port_id, uint32_t latency)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            status_t backend_t::connect_ports(audio::backend_t *self, const char *source, const char *destination)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            status_t backend_t::disconnect_ports(audio::backend_t *self, const char *source, const char *destination)
            {
                return STATUS_NOT_IMPLEMENTED;
            }

            size_t backend_t::audio_buffers_count(audio::backend_t *self, port_id_t port_id)
            {
                return 0;
            }

            float *backend_t::get_audio_buffer(audio::backend_t *self, port_id_t port_id, size_t index)
            {
                return NULL;
            }

            size_t backend_t::midi_events_count(audio::backend_t *self, port_id_t port_id)
            {
                return 0;
            }

            status_t backend_t::read_midi_event(audio::backend_t *self, port_id_t port_id, midi_event_t *event, uint32_t index)
            {
                return STATUS_NO_DATA;
            }

            uint8_t *backend_t::write_midi_event(audio::backend_t *self, port_id_t port_id, uint32_t timestamp, uint32_t size)
            {
                return NULL;
            }

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */




