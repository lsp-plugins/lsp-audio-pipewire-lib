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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_BACKEND_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_BACKEND_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/iface/backend.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {

            typedef struct LSP_HIDDEN_MODIFIER backend_t: public audio::backend_t
            {
                public:
                    void               *pUserData;
                    const callbacks_t  *pCallbacks;
                    io_parameters_t     sIOParams;
                    io_position_t       sIOPosition;
                    uint32_t            nLatency;

                public:
                    explicit            backend_t();
                    void                construct();

                public:
                    static status_t     connect(
                        audio::backend_t *self,
                        const connection_params_t *params,
                        const callbacks_t *callbacks,
                        void *user_data);
                    static status_t     set_latency(audio::backend_t *self, uint32_t latency);
                    static status_t     disconnect(audio::backend_t *self);
                    static void         destroy(audio::backend_t *self);

                    static port_id_t    register_port(audio::backend_t *self, const char *id, uint32_t flags);
                    static status_t     unregister_port(audio::backend_t *self, port_id_t port_id);
                    static const char  *port_system_name(audio::backend_t *self, port_id_t port_id);
                    static status_t     set_port_latency(audio::backend_t *self, port_id_t port_id, uint32_t latency);

                    static status_t     connect_ports(audio::backend_t *self, const char *source, const char *destination);
                    static status_t     disconnect_ports(audio::backend_t *self, const char *source, const char *destination);

                    static size_t       audio_buffers_count(audio::backend_t *self, port_id_t port_id);
                    static float       *get_audio_buffer(audio::backend_t *self, port_id_t port_id, size_t index);

                    static size_t       midi_events_count(audio::backend_t *self, port_id_t port_id);
                    static status_t     read_midi_event(audio::backend_t *self, port_id_t port_id, midi_event_t *event, uint32_t index);
                    static uint8_t     *write_midi_event(audio::backend_t *self, port_id_t port_id, uint32_t timestamp, uint32_t size);

            } backend_t;
        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_BACKEND_H_ */
