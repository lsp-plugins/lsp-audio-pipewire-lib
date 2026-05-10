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
#include <lsp-plug.in/audio/pipewire/memmap.h>
#include <lsp-plug.in/audio/pipewire/mutex.h>
#include <lsp-plug.in/audio/pipewire/port_map.h>
#include <lsp-plug.in/audio/pipewire/registry.h>
#include <lsp-plug.in/audio/pipewire/ringbuffer.h>

#include <pipewire/pipewire.h>
#include <pipewire/extensions/client-node.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/latency.h>
#include <spa/utils/ringbuffer.h>

#if (PW_VERSION_CORE >= 4) && (PW_VERSION_CORE_EVENTS >= 1)
    #define PIPEWIRE_HAS_BOUND_PROPS
#endif /* PIPEWIRE_HAS_BOUND_PROPS */

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            /**
             * Trivially-movable backend
             */
            typedef struct LSP_HIDDEN_MODIFIER backend_t: public audio::backend_t
            {
                protected:
                    static constexpr size_t MAX_PORT_ID_BYTES   = 16;

                    enum spa_hook_type_t
                    {
                        HOOK_CORE,
                        HOOK_REGISTRY,
                        HOOK_FILTER,
                        HOOK_METADATA_PROXY,
                        HOOK_METADATA,

                        HOOK_TOTAL
                    };

                    struct port_data_t;

                    typedef struct port_t
                    {
                        uint32_t            nType;
                        port_data_t        *pHandle;
                        pw_buffer          *pBuffer;
                        char               *sFullId;
                        char                sID[MAX_PORT_ID_BYTES];
                    } port_t;

                    typedef struct port_data_t
                    {
                        port_id_t           nPortId;
                    } port_data_t;

                    typedef struct control_header_t
                    {
                        uint32_t            timestamp;
                        uint32_t            type;
                    } control_header_t;

                    typedef memmap<spa_io_position>     mm_io_position;
                    typedef memmap<spa_io_clock>        mm_io_clock;

                protected:
                    static const pw_registry_events         registry_events;
                    static const pw_core_events             core_events;
                    static const pw_filter_events           filter_events;
                    static const spa_thread_utils_methods   thread_utils_impl;
                    static const pw_proxy_events            metadata_proxy_events;
                    static const pw_metadata_events         metadata_events;
                    static const pw_proxy_events            link_proxy_events;

                public:
                    char               *sClientName;
                    char               *sServerName;
                    mutex_t            *pMutex;
                    pw_data_loop       *pAudioDataLoop;
                    pw_thread_loop     *pContextThreadLoop;
                    pw_loop            *pAudioLoop;
                    pw_loop            *pContextLoop;
                    pw_context         *pContext;
                    pw_core            *pCore;
                    pw_mempool         *pMemPool;
                    pw_registry        *pRegistry;
                    pw_filter          *pFilter;
                    pw_metadata        *pMetadata;
                    spa_thread_utils   *pOldThreadUtils;

                    registry            sRegistry;
                    ringbuffer          sRingBuffer;
                    dictionary          sClientDict;
                    dictionary          sContextDict;
                    spa_thread_utils    sThreadUtils;
                    spa_hook            vHooks[HOOK_TOTAL];

                    void               *pUserData;
                    const callbacks_t  *pCallbacks;
                    port_t             *vPorts;
                    port_id_t           nPortFirst;
                    port_id_t           nPortCapacity;
                    io_parameters_t     sIOParams;
                    io_position_t       sIOPosition;
                    uint32_t            nLatency;
                    int                 nSyncRequestId;
                    int                 nSyncResponseId;
                    int                 nSyncError;
                    bool                bActivated;

                public:
                    explicit            backend_t() noexcept;
                    void                construct() noexcept;

                protected:
                    // PipeWire registry events
                    static void         on_registry_event_global(
                        void *self, uint32_t id,
                        uint32_t permissions, const char *type, uint32_t version,
                        const spa_dict *props);
                    static void         on_registry_event_removed(void *self, uint32_t id);

                protected:
                    // PipeWire core events
                    static void         on_core_info(void *self, const struct pw_core_info *info);
                    static void         on_core_done(void *self, uint32_t id, int seq);
                    static void         on_core_ping(void *self, uint32_t id, int seq);
                    static void         on_core_error(void *self, uint32_t id, int seq, int res, const char *message);
                    static void         on_core_remove_id(void *self, uint32_t id);
                    static void         on_core_bound_id(void *self, uint32_t id, uint32_t global_id);
                    static void         on_core_add_mem(void *self, uint32_t id, uint32_t type, int fd, uint32_t flags);
                    static void         on_core_remove_mem(void *self, uint32_t id);
                #ifdef PIPEWIRE_HAS_BOUND_PROPS
                    static void         on_core_bound_props(void *self, uint32_t id, uint32_t global_id, const struct spa_dict *props);
                #endif /* PIPEWIRE_HAS_BOUND_PROPS */

                protected:
                    static void         on_filter_destroy(void *self);
                    static void         on_filter_state_changed(void *self, enum pw_filter_state old, enum pw_filter_state state, const char *error);
                    static void         on_filter_io_changed(void *self, void *port_data, uint32_t id, void *area, uint32_t size);
                    static void         on_filter_param_changed(void *self, void *port_data, uint32_t id, const struct spa_pod *param);
                    static void         on_filter_add_buffer(void *self, void *port_data, struct pw_buffer *buffer);
                    static void         on_filter_remove_buffer(void *self, void *port_data, struct pw_buffer *buffer);
                    static void         on_filter_process(void *self, struct spa_io_position *position);
                    static void         on_filter_drained(void *self);
                    static void         on_filter_command(void *self, const struct spa_command *command);

                protected:
                    // PipeWire thread utils methods
                    static spa_thread  *on_thread_create(void *self, const spa_dict *props, void *(*start)(void*), void *arg);
                    static int          on_thread_join(void *self, struct spa_thread *thread, void **retval);
                    static int          on_thread_get_rt_range(void *self, const struct spa_dict *props, int *min, int *max);
                    static int          on_thread_acquire_rt(void *self, struct spa_thread *thread, int priority);
                    static int          on_thread_drop_rt(void *self, struct spa_thread *thread);

                protected:
                    // PipeWire metadata events
                    static void         on_metadata_destroy(void *self);
                    static void         on_metadata_removed(void *self);
                    static int          on_metadata_property(void *self, uint32_t subject, const char *key, const char *type, const char *value);

                protected:
                    // PipeWire link proxy events
                    static void         on_link_error(void *code, int seq, int res, const char *message);

                protected:
                    // PipeWire notification source
                    static void         on_notify_event(void *self, uint64_t count);

                protected:
                    // PipeWire ringbuffer events
                    static void         on_ringbuffer_data_received(void *self, uint64_t count);

                protected:
                    // PipeWire miscellaneous processing
                    static int          execute_context_properties_match(void *self, const char *location, const char *action, const char *val, size_t len);

                protected:
                    int                 sync_core(bool lock) noexcept;
                    status_t            make_connection(
                        const connection_params_t *params,
                        const callbacks_t *callbacks,
                        void *user_data) noexcept;
                    status_t            activate() noexcept;
                    status_t            deactivate() noexcept;
                    void                close_connection() noexcept;
                    status_t            register_port(port_t *port) noexcept;
                    status_t            unregister_port(port_t *port) noexcept;
                    status_t            register_ports() noexcept;
                    void                unregister_ports() noexcept;
                    void                update_sample_rate(const struct spa_pod *param) noexcept;
                    void                notify_connection_lost(bool stop) noexcept;
                    void                update_latency(uint32_t latency) noexcept;

                protected:
                    static void         init_port(port_t *port) noexcept;
                    port_t             *find_port(const char *id) noexcept;
                    void                free_port(port_t *port) noexcept;
                    port_t             *alloc_port(const char *id, uint32_t flags) noexcept;
                    char               *make_port_full_id(const char *id) const noexcept;

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

                    static status_t     connect_ports(audio::backend_t *self, const char *source, const char *destination);
                    static status_t     disconnect_ports(audio::backend_t *self, const char *source, const char *destination);

                    static size_t       audio_buffers_count(audio::backend_t *self, port_id_t port_id);
                    static float       *get_audio_buffer(audio::backend_t *self, port_id_t port_id, size_t index);

                    static status_t     read_midi_event(audio::backend_t *self, port_id_t port_id, midi_event_t *event, uint32_t *index);
                    static uint8_t     *write_midi_event(audio::backend_t *self, port_id_t port_id, uint32_t timestamp, uint32_t size);

            } backend_t;
        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_BACKEND_H_ */
