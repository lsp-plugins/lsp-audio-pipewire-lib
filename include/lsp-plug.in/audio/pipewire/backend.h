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
#include <lsp-plug.in/audio/pipewire/dictionary.h>
#include <lsp-plug.in/ipc/Mutex.h>

#include <pipewire/pipewire.h>
#include <pipewire/extensions/client-node.h>

#if (PW_VERSION_CORE >= 4) && (PW_VERSION_CORE_EVENTS >= 1)
    #define PIPEWIRE_HAS_BOUND_PROPS
#endif /* PIPEWIRE_HAS_BOUND_PROPS */

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {

            typedef struct LSP_HIDDEN_MODIFIER backend_t: public audio::backend_t
            {
                protected:
                    enum spa_hook_type
                    {
                        HOOK_CORE,
                        HOOK_REGISTRY,
                        HOOK_NODE,
                        HOOK_NODE_PROXY,

                        HOOK_TOTAL
                    };

                protected:
                    static const pw_registry_events         registry_events;
                    static const pw_core_events             core_events;
                    static const pw_client_node_events      node_events;
                    static const pw_proxy_events            node_proxy_events;
                    static const spa_thread_utils_methods   thread_utils_impl;

                public:
                    char               *sClientName;
                    char               *sServerName;
                    ipc::Mutex         *pDataMutex;
                    pw_data_loop       *pAudioDataLoop;
                    pw_thread_loop     *pContextThreadLoop;
                    pw_thread_loop     *pNotifyThreadLoop;
                    pw_loop            *pAudioLoop;
                    pw_loop            *pContextLoop;
                    pw_loop            *pNotifyLoop;
                    pw_context         *pContext;
                    pw_core            *pCore;
                    pw_registry        *pRegistry;
                    pw_client_node     *pNode;
                    spa_thread_utils   *pOldThreadUtils;

                    prop_dict           sClientProps;
                    prop_dict           sContextProps;
                    spa_thread_utils    sThreadUtils;
                    spa_node_info       sNodeInfo;
                    spa_hook            vHooks[HOOK_TOTAL];

                    void               *pUserData;
                    const callbacks_t  *pCallbacks;
                    io_parameters_t     sIOParams;
                    io_position_t       sIOPosition;
                    uint32_t            nLatency;

                public:
                    explicit            backend_t();
                    void                construct();

                protected:
                    // PipeWire registry events
                    static void on_registry_event_global(
                        void *self, uint32_t id,
                        uint32_t permissions, const char *type, uint32_t version,
                        const spa_dict *props);
                    static void on_registry_event_removed(void *self, uint32_t id);

                protected:
                    // PipeWire core events
                    static void on_core_info(void *self, const struct pw_core_info *info);
                    static void on_core_done(void *self, uint32_t id, int seq);
                    static void on_core_ping(void *self, uint32_t id, int seq);
                    static void on_core_error(void *self, uint32_t id, int seq, int res, const char *message);
                    static void on_core_remove_id(void *self, uint32_t id);
                    static void on_core_bound_id(void *self, uint32_t id, uint32_t global_id);
                    static void on_core_add_mem(void *self, uint32_t id, uint32_t type, int fd, uint32_t flags);
                    static void on_core_remove_mem(void *self, uint32_t id);
                #ifdef PIPEWIRE_HAS_BOUND_PROPS
                    static void on_core_bound_props(void *self, uint32_t id, uint32_t global_id, const struct spa_dict *props);
                #endif /* PIPEWIRE_HAS_BOUND_PROPS */

                protected:
                    // PipeWire node events
                    static int on_node_transport(void *self, int readfd, int writefd, uint32_t mem_id, uint32_t offset, uint32_t size);
                    static int on_node_set_param(void *self, uint32_t id, uint32_t flags, const spa_pod *param);
                    static int on_node_set_io(void *self, uint32_t id, uint32_t mem_id, uint32_t offset, uint32_t size);
                    static int on_node_event(void *self, const struct spa_event *event);
                    static int on_node_command(void *self, const struct spa_command *command);
                    static int on_node_add_port(void *self, spa_direction direction, uint32_t port_id, const spa_dict *props);
                    static int on_node_remove_port(void *self, spa_direction direction, uint32_t port_id);
                    static int on_node_port_set_param(void *self, spa_direction direction, uint32_t port_id, uint32_t id, uint32_t flags, const spa_pod *param);
                    static int on_node_port_use_buffers(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t flags, uint32_t n_buffers, pw_client_node_buffer *buffers);
                    static int on_node_port_set_io(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t id, uint32_t mem_id, uint32_t offset, uint32_t size);
                    static int on_node_set_activation(void *self, uint32_t node_id, int signalfd, uint32_t mem_id, uint32_t offset, uint32_t size);
                    static int on_node_port_set_mix_info(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t peer_id, const spa_dict *props);

                protected:
                    // PipeWire node proxy events
                    static void on_node_destroy(void *self);
                    static void on_node_bound(void *self, uint32_t global_id);
                    static void on_node_removed(void *self);
                    static void on_node_done(void *self, int seq);
                    static void on_node_error(void *self, int seq, int res, const char *message);
                    static void on_node_bound_props(void *self, uint32_t global_id, const struct spa_dict *props);

                protected:
                    // PipeWire thread utils methods
                    static spa_thread * on_thread_create(void *self, const spa_dict *props, void *(*start)(void*), void *arg);
                    static int on_thread_join(void *self, struct spa_thread *thread, void **retval);
                    static int on_thread_get_rt_range(void *self, const struct spa_dict *props, int *min, int *max);
                    static int on_thread_acquire_rt(void *self, struct spa_thread *thread, int priority);
                    static int on_thread_drop_rt(void *self, struct spa_thread *thread);

                protected:
                    // PipeWire miscellaneous processing
                    static int execute_context_properties_match(void *self, const char *location, const char *action, const char *val, size_t len);

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
