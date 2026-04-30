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

#include <lsp-plug.in/common/atomic.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/common/status.h>

#include <lsp-plug.in/stdlib/string.h>
#include <lsp-plug.in/audio/pipewire/backend.h>

#include <spa/support/thread.h>
#include <pipewire/thread.h>

#ifndef SPA_KEY_THREAD_RESET_ON_FORK
    #define SPA_KEY_THREAD_RESET_ON_FORK    "thread.reset-on-fork"  /* reset priority and policy for real-time threads on fork. Default true */
#endif /* SPA_KEY_THREAD_RESET_ON_FORK */

#ifndef PW_KEY_LOOP_CANCEL
    #define PW_KEY_LOOP_CANCEL              "loop.cancel"
#endif /* PW_KEY_LOOP_CANCEL */

#include <stdlib.h>
#include <errno.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            static constexpr size_t NOTIFY_BUFFER_SIZE          = 1 << 16;
            static constexpr size_t NOTIFY_BUFFER_MASK          = NOTIFY_BUFFER_SIZE - 1;

            static const char *prop_true    = "true";
            static const char *prop_false   = "false";

            // PipeWire core events
            const pw_core_events backend_t::core_events = {
                .version = PW_VERSION_CORE_EVENTS,
                .info = on_core_info,
                .done = on_core_done,
                .ping = on_core_ping,
                .error = on_core_error,
                .remove_id = on_core_remove_id,
                .bound_id = on_core_bound_id,
                .add_mem = on_core_add_mem,
                .remove_mem = on_core_remove_mem,
            #ifdef PIPEWIRE_HAS_BOUND_PROPS
                .bound_props = on_core_bound_props,
            #endif /* PIPEWIRE_HAS_BOUND_PROPS */
            };

            // PipeWire registry events
            const pw_registry_events backend_t::registry_events =
            {
                .version            = PW_VERSION_REGISTRY_EVENTS,
                .global             = on_registry_event_global,
                .global_remove      = on_registry_event_removed,
            };

            // PipeWire node events
            const pw_client_node_events backend_t::node_events =
            {
                .version            = PW_VERSION_CLIENT_NODE_EVENTS,
                .transport          = on_node_transport,
                .set_param          = on_node_set_param,
                .set_io             = on_node_set_io,
                .event              = on_node_event,
                .command            = on_node_command,
                .add_port           = on_node_add_port,
                .remove_port        = on_node_remove_port,
                .port_set_param     = on_node_port_set_param,
                .port_use_buffers   = on_node_port_use_buffers,
                .port_set_io        = on_node_port_set_io,
                .set_activation     = on_node_set_activation,
                .port_set_mix_info  = on_node_port_set_mix_info
            };

            // PipeWire node proxy events
            const pw_proxy_events backend_t::node_proxy_events =
            {
                .version            = PW_VERSION_PROXY_EVENTS,
                .destroy            = on_node_destroy,
                .bound              = on_node_bound,
                .removed            = on_node_removed,
                .done               = on_node_done,
                .error              = on_node_error,
                .bound_props        = on_node_bound_props,
            };

            // PipeWire thread utils implementation
            const spa_thread_utils_methods backend_t::thread_utils_impl =
            {
                .version            = SPA_VERSION_THREAD_UTILS_METHODS,
                .create             = on_thread_create,
                .join               = on_thread_join,
                .get_rt_range       = on_thread_get_rt_range,
                .acquire_rt         = on_thread_acquire_rt,
                .drop_rt            = on_thread_drop_rt,
            };

            // Miscellaneous functions
//            static constexpr uint32_t PORT_TYPE_FREE        = 0xffffffff;
//            static constexpr uint32_t PORT_MASK_ALL         = PORT_DIR_MASK | PORT_TYPE_MASK;

            static inline pipewire::backend_t *cast(audio::backend_t *self)
            {
                return static_cast<pipewire::backend_t *>(self);
            }

            static inline pipewire::backend_t *cast(void *self)
            {
                return static_cast<pipewire::backend_t *>(self);
            }

            template <typename T>
            static inline pw_proxy *to_pw_proxy(T *arg)
            {
                return reinterpret_cast<pw_proxy *>(arg);
            }

            // Backend implementation
            backend_t::backend_t()
            {
                construct();
            }

            void backend_t::construct()
            {
                sClientName                     = NULL;
                sServerName                     = NULL;
//                pDataMutex                      = NULL;
                pAudioDataLoop                  = NULL;
                pContextThreadLoop              = NULL;
                pNotifyThreadLoop               = NULL;
                pAudioLoop                      = NULL;
                pContextLoop                    = NULL;
                pNotifyLoop                     = NULL;
                pContext                        = NULL;
                pNotifySource                   = NULL;
                pCore                           = NULL;
                pMemPool                        = NULL;
                pRegistry                       = NULL;
                pNode                           = NULL;
                pOldThreadUtils                 = NULL;

                sClientDict.construct();
                sContextDict.construct();
                bzero(&sThreadUtils, sizeof(sThreadUtils));
                bzero(&sNodeInfo, sizeof(sNodeInfo));
                bzero(&sNotifyRing, sizeof(sNotifyRing));
                bzero(vHooks, sizeof(spa_hook) * HOOK_TOTAL);

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
                nRequestedSeq                   = 0;
                nRequestedSync                  = -EINVAL;
                atomic_store(&nReceivedSync, -EINVAL);

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
                int error;
                status_t res;
                backend_t * const back = cast(self);

                // Check that backend is disconnected
                if (back->pAudioLoop != NULL)
                    return STATUS_BAD_STATE;

                if (params->client_name == NULL)
                    return STATUS_BAD_ARGUMENTS;

                // Set-up destruction hook
                bool success = false;
                lsp_finally {
                    if (!success)
                        disconnect(self);
                };

                // Fill client name
                back->sClientName   = strdup(params->client_name);
                if (back->sClientName == NULL)
                {
                    lsp_warn("Failed to allocate client name string");
                    return STATUS_NO_MEM;
                }

                // Fill server name
                if ((params->url != NULL) &&
                    ((strlen(params->url) == 0) ||
                    (strcmp(params->url, "default") != 0)))
                {
                    back->sServerName   = strdup(params->url);
                    if (back->sServerName == NULL)
                    {
                        lsp_warn("Failed to allocate server name string");
                        return STATUS_NO_MEM;
                    }
                }

                // Allocate notification buffer
                back->pNotifyBuffer     = calloc(1, NOTIFY_BUFFER_SIZE);
                if (back->pNotifyBuffer == NULL)
                {
                    lsp_warn("Failed to allocate notification buffer");
                    return STATUS_NO_MEM;
                }

                // Create mutex
//                back->pDataMutex = new ipc::Mutex();
//                if (back->pDataMutex == NULL)
//                    return STATUS_NO_MEM;

                // Create context properties
                res = back->sClientDict.put(
                    PW_KEY_LOOP_CANCEL, prop_true,
                    SPA_KEY_THREAD_RESET_ON_FORK, prop_false,
                    PW_KEY_REMOTE_NAME, back->sServerName,
                    PW_KEY_CLIENT_NAME, back->sClientName,
                    PW_KEY_CLIENT_API, "native",
                    PW_KEY_CONFIG_NAME, "client.conf");
                if (res != STATUS_OK)
                {
                    lsp_warn("Failed to initialize client properties, code=%d", int(res));
                    return res;
                }

                // Create context thread loop
                back->pContextThreadLoop = pw_thread_loop_new(back->sClientName, NULL);
                if (back->pContextThreadLoop == NULL)
                {
                    lsp_warn("Failed to create PipeWire context thread loop");
                    return STATUS_UNKNOWN_ERR;
                }
                back->pContextLoop = pw_thread_loop_get_loop(back->pContextThreadLoop);

                // Create context
                {
                    lsp_trace("Context properties:\n%s\n", back->sClientDict.to_string());

                    pw_properties * const context_props = back->sClientDict.make_properties();
                    if (context_props == NULL)
                    {
                        lsp_warn("Failed to create context properties");
                        return STATUS_NO_MEM;
                    }
                    back->pContext = pw_context_new(back->pContextLoop, context_props, 0);
                    if (back->pContext == NULL)
                    {
                        lsp_warn("Failed to create PipeWire context");
                        pw_properties_free(context_props);
                        return STATUS_NO_MEM;
                    }
                }

                // Create notify thread loop
                back->pNotifyThreadLoop = pw_thread_loop_new(back->sClientName, NULL);
                if (back->pNotifyThreadLoop == NULL)
                {
                    lsp_warn("Failed to create PipeWire notification thread loop");
                    return STATUS_DISCONNECTED;
                }
                back->pNotifyLoop       = pw_thread_loop_get_loop(back->pNotifyThreadLoop);
                back->pNotifySource     = pw_loop_add_event(back->pNotifyLoop, on_notify_event, back);
                if (back->pNotifySource == NULL)
                {
                    lsp_warn("Failed to add PipeWire notification source");
                    return STATUS_DISCONNECTED;
                }
                spa_ringbuffer_init(&back->sNotifyRing);

                // Update client properties
                {
                    pw_properties * const client_props = back->sClientDict.make_properties();
                    if (client_props == NULL)
                    {
                        lsp_warn("Failed to allocate client properties");
                        return STATUS_NO_MEM;
                    }
                    lsp_finally { pw_properties_free(client_props); };

                    error = pw_context_conf_update_props(back->pContext, "filter.properties", client_props);
                    if (error < 0)
                    {
                        lsp_warn("Failed to fetch context properties: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    res = back->sClientDict.put(client_props);
                    if (res != STATUS_OK)
                    {
                        lsp_warn("Failed to synchronize client dictionary, code=%d", int(res));
                        return res;
                    }

                    lsp_trace("Client dictionary:\n%s\n", back->sClientDict.to_string());
                }

                // Fetch context properties
                {
                    const pw_properties * const context_props = pw_context_get_properties(back->pContext);
                    if (context_props != NULL)
                    {
                        res = back->sContextDict.put(context_props);
                        if (res != STATUS_OK)
                        {
                            lsp_warn("Failed to synchronize context dictionary, code=%d", int(res));
                            return res;
                        }

                        pw_context_conf_section_match_rules(
                            back->pContext, "client.rules",
                            back->sContextDict.dict(), execute_context_properties_match, self);
                    }
                    lsp_trace("Context dictionary:\n%s", back->sContextDict.to_string());
                }

                // Thread utils interface
                back->pOldThreadUtils   = static_cast<spa_thread_utils *>(pw_context_get_object(back->pContext, SPA_TYPE_INTERFACE_ThreadUtils));
                if (back->pOldThreadUtils == NULL)
                    back->pOldThreadUtils = pw_thread_utils_get();

                back->sThreadUtils      = spa_thread_utils {
                    SPA_INTERFACE_INIT(
                        SPA_TYPE_INTERFACE_ThreadUtils,
                        SPA_VERSION_THREAD_UTILS,
                        &thread_utils_impl, self)
                };

                pw_context_set_object(
                    back->pContext,
                    SPA_TYPE_INTERFACE_ThreadUtils,
                    &back->sThreadUtils);

                // Stop audio data loop
                back->pAudioDataLoop    = pw_context_get_data_loop(back->pContext);
                back->pAudioLoop        = pw_data_loop_get_loop(back->pAudioDataLoop);
                pw_data_loop_stop(back->pAudioDataLoop);

                // Start context thread loop
                error = pw_thread_loop_start(back->pContextThreadLoop);
                if (error < 0)
                {
                    lsp_warn("Failed to start context thread loop: code=%d", -error);
                    return STATUS_DISCONNECTED;
                }

                {
                    pw_thread_loop_lock(back->pContextThreadLoop);
                    lsp_finally { pw_thread_loop_unlock(back->pContextThreadLoop); };

                    // Connect to PipeWire Core and add Core listener
                    {
                        pw_properties * const core_properties = back->sClientDict.make_properties();
                        if (core_properties == NULL)
                            return STATUS_NO_MEM;
                        back->pCore             = pw_context_connect(back->pContext, core_properties, 0);
                        if (back->pCore == NULL)
                        {
                            lsp_warn("Could not connect to PipeWire");
                            pw_properties_free(core_properties);
                            return STATUS_DISCONNECTED;
                        }
                    }
                    error = pw_core_add_listener(back->pCore, &back->vHooks[HOOK_CORE], &core_events, back);
                    if (error < 0)
                    {
                        lsp_warn("Failed to add core listener: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    // Get memory pool
                    back->pMemPool  = pw_core_get_mempool(back->pCore);
                    if (back->pMemPool == NULL)
                    {
                        lsp_warn("Could not obtain memory pool");
                        return STATUS_DISCONNECTED;
                    }

                    // Get Registry and add Registry listener
                    back->pRegistry = pw_core_get_registry(back->pCore, PW_VERSION_REGISTRY, 0);
                    if (back->pRegistry == NULL)
                    {
                        lsp_warn("Failed to obtain PipeWire registry");
                        return STATUS_DISCONNECTED;
                    }
                    error = pw_registry_add_listener(back->pRegistry, &back->vHooks[HOOK_REGISTRY], &registry_events, back);
                    if (error < 0)
                    {
                        lsp_warn("Failed to add registry listener: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    // Setup properties
                    {
                        const char *key_node_group = back->sContextDict.value(
                            PW_KEY_NODE_GROUP,
                            back->sClientDict.value(
                                PW_KEY_NODE_GROUP, "group.dsp.0"));
                        res = back->sClientDict.put(
                            PW_KEY_NODE_NAME, back->sClientName,
                            PW_KEY_NODE_GROUP, key_node_group,
                            PW_KEY_NODE_DESCRIPTION, back->sClientName,
                            PW_KEY_MEDIA_TYPE, "Audio",
                            PW_KEY_MEDIA_CATEGORY, "Duplex",
                            PW_KEY_MEDIA_ROLE, "DSP",
                            PW_KEY_NODE_ALWAYS_PROCESS, prop_true,
                            PW_KEY_NODE_LOCK_QUANTUM, prop_true);
                        if (res != STATUS_OK)
                        {
                            lsp_warn("Failed to synchronize client dictionary, code=%d", int(res));
                            return res;
                        }

                        lsp_trace("Node dictionary:\n%s", back->sClientDict.to_string());
                    }

                    // Create client node
                    back->pNode = static_cast<pw_client_node *>(
                        pw_core_create_object(
                            back->pCore,
                            "client-node",
                            PW_TYPE_INTERFACE_ClientNode,
                            PW_VERSION_CLIENT_NODE,
                            back->sClientDict.dict(),
                            0));
                    if (back->pNode == NULL)
                    {
                        lsp_warn("Could not create PipeWire node");
                        return STATUS_DISCONNECTED;
                    }
                    error = pw_client_node_add_listener(back->pNode, &back->vHooks[HOOK_NODE], &node_events, back);
                    if (error < 0)
                    {
                        lsp_warn("Failed to add node listener: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }
                    pw_proxy_add_listener(
                        reinterpret_cast<pw_proxy *>(back->pNode),
                        &back->vHooks[HOOK_NODE_PROXY], &node_proxy_events, back);

                    // Update node information
                    back->sNodeInfo = SPA_NODE_INFO_INIT();
                    back->sNodeInfo.max_input_ports = UINT32_MAX;
                    back->sNodeInfo.max_output_ports = UINT32_MAX;
                    back->sNodeInfo.change_mask = SPA_NODE_CHANGE_MASK_FLAGS | SPA_NODE_CHANGE_MASK_PROPS;
                    back->sNodeInfo.flags = SPA_NODE_FLAG_RT;
                    back->sNodeInfo.props = const_cast<spa_dict *>(back->sClientDict.dict());

                    error = pw_client_node_update(
                        back->pNode,
                        PW_CLIENT_NODE_UPDATE_INFO,
                        0, NULL, &back->sNodeInfo);
                    if (error < 0)
                    {
                        lsp_warn("Failed to update client node, code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    back->sNodeInfo.change_mask = 0;

                    // Issue sync request
                    back->nRequestedSeq         = 0;
                    back->nRequestedSync        = -EINVAL;
                    atomic_store(&back->nReceivedSync, -EINVAL);

                    back->request_sync();
                    error = back->wait_sync_complete();
                    if (error < 0)
                    {
                        lsp_warn("Failed to synchrnonize with PipeWire server: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }
                }

                // Start notification thread loop
                error = pw_thread_loop_start(back->pNotifyThreadLoop);
                if (error < 0)
                {
                    lsp_warn("Failed to start notify thread loop: code=%d", -error);
                    return STATUS_DISCONNECTED;
                }

                // Return success result
                success             = true;

                return STATUS_OK;
            }

            status_t backend_t::disconnect(audio::backend_t *self)
            {
                backend_t * const back = cast(self);

                if (back->pContextThreadLoop != NULL)
                {
                    // Destroy all related objects
                    {
                        pw_thread_loop_lock(back->pContextThreadLoop);
                        lsp_finally { pw_thread_loop_unlock(back->pContextThreadLoop); };

                        // Destroy node
                        if (back->pNode != NULL)
                        {
                            spa_hook_remove(&back->vHooks[HOOK_NODE_PROXY]);
                            spa_hook_remove(&back->vHooks[HOOK_NODE]);
                            pw_proxy_destroy(to_pw_proxy(back->pNode));
                        }

                        // Destroy registry
                        if (back->pRegistry != NULL)
                        {
                            spa_hook_remove(&back->vHooks[HOOK_REGISTRY]);
                            pw_proxy_destroy(to_pw_proxy(back->pRegistry));
                            back->pRegistry = NULL;
                        }

                        // Destro core
                        if (back->pCore != NULL)
                        {
                            spa_hook_remove(&back->vHooks[HOOK_CORE]);
                            pw_core_disconnect(back->pCore);
                            back->pCore     = NULL;
                        }
                        back->pMemPool  = NULL;

                        // Destroy context
                        if (back->pContext != NULL)
                        {
                            pw_context_destroy(back->pContext);
                            back->pContext = NULL;
                        }
                    }

                    pw_thread_loop_stop(back->pContextThreadLoop);
                }

                back->pAudioLoop = NULL;
                back->pAudioDataLoop = NULL;

                if (back->pNotifyThreadLoop != NULL)
                {
                    pw_thread_loop_destroy(back->pNotifyThreadLoop);
                    back->pNotifyThreadLoop = NULL;
                    back->pNotifyLoop = NULL;
                }

                if (back->pContextThreadLoop != NULL)
                {
                    pw_thread_loop_destroy(back->pContextThreadLoop);
                    back->pContextThreadLoop = NULL;
                    back->pContextLoop = NULL;
                }

//                if (back->pDataMutex != NULL)
//                {
//                    delete back->pDataMutex;
//                    back->pDataMutex = NULL;
//                }

                if (back->pNotifyBuffer != NULL)
                {
                    free(back->pNotifyBuffer);
                    back->pNotifyBuffer = NULL;
                }

                if (back->sServerName != NULL)
                {
                    free(back->sServerName);
                    back->sServerName = NULL;
                }

                if (back->sClientName != NULL)
                {
                    free(back->sClientName);
                    back->sClientName = NULL;
                }

                back->sClientDict.destroy();
                back->sContextDict.destroy();
                bzero(&back->sThreadUtils, sizeof(back->sThreadUtils));
                bzero(&back->sNodeInfo, sizeof(back->sNodeInfo));
                bzero(&back->sNotifyRing, sizeof(back->sNotifyRing));
                bzero(back->vHooks, sizeof(spa_hook) * HOOK_TOTAL);

                return STATUS_NOT_IMPLEMENTED;
            }

            int backend_t::request_sync()
            {
                if (pCore == NULL)
                    return -EINVAL;

                pw_thread_loop_lock(pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };
                nRequestedSync = pw_proxy_sync(to_pw_proxy(pCore), ++nRequestedSeq);

                return nRequestedSync;
            }

            int backend_t::wait_sync_complete()
            {
                if (nRequestedSync < 0)
                    return nRequestedSync;

                int received_sync = atomic_load(&nReceivedSync);
                if (received_sync == nRequestedSync)
                    return received_sync;

                pw_thread_loop_lock(pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                do {
                    pw_thread_loop_wait(pContextThreadLoop);
                    received_sync = atomic_load(&nReceivedSync);
                } while ((received_sync >= 0) && (received_sync != nRequestedSync));

                return received_sync;
            }

            status_t backend_t::set_latency(audio::backend_t *self, uint32_t latency)
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

            // PipeWire Registry callbacks
            void backend_t::on_registry_event_global(
                void *self, uint32_t id,
                uint32_t permissions, const char *type, uint32_t version,
                const spa_dict *props)
            {
                lsp_trace("self=%p, id=%d, permissions=0x%x, type:%s/%d, props=%p\n",
                    self, int(id), int(permissions), type, int(version), props);
            }

            void backend_t::on_registry_event_removed(void *self, uint32_t id)
            {
                lsp_trace("self=%p, id=%d\n", self, int(id));
            }

            // PipeWire Core callbacks
            void backend_t::on_core_info(void *self, const struct pw_core_info *info)
            {
                lsp_trace(
                    "self=%p, id=%d, cookie=%d, "
                    "user_name='%s', host_name='%s', version='%s', name='%s', "
                    "change_mask=0x%llx, props=%p\n",
                    self, int(info->id), int(info->cookie),
                    info->user_name, info->host_name, info->version, info->name,
                    static_cast<long long>(info->change_mask), info->props);
            }

            void backend_t::on_core_done(void *self, uint32_t id, int seq)
            {
                lsp_trace("self=%p, id=%d, seq=%d", self, int(id), int(seq));

                backend_t * const back = cast(self);
                if (id != PW_ID_CORE)
                    return;

                atomic_store(&back->nReceivedSync, seq);
                if (back->nRequestedSync == seq)
                    pw_thread_loop_signal(back->pContextThreadLoop, false);
            }

            void backend_t::on_core_ping(void *self, uint32_t id, int seq)
            {
                lsp_trace("self=%p, id=%d, seq=%d", self, int(id), int(seq));
            }

            void backend_t::on_core_error(void *self, uint32_t id, int seq, int res, const char *message)
            {
                lsp_trace("self=%p, id=%d, seq=%d, res=%d, message='%s'",
                    self, int(id), int(seq), int(res), message);
            }

            void backend_t::on_core_remove_id(void *self, uint32_t id)
            {
                lsp_trace("self=%p, id=%d", self, int(id));
            }

            void backend_t::on_core_bound_id(void *self, uint32_t id, uint32_t global_id)
            {
                lsp_trace("self=%p, id=%d, global_id=%d", self, int(id), int(global_id));
            }

            void backend_t::on_core_add_mem(void *self, uint32_t id, uint32_t type, int fd, uint32_t flags)
            {
                lsp_trace("self=%p, id=%d, type=%d, fd=%d, flags=0x%x",
                    self, int(id), int(type), fd, int(flags));
            }

            void backend_t::on_core_remove_mem(void *self, uint32_t id)
            {
                lsp_trace("self=%p, id=%d", self, int(id));
            }

        #ifdef PIPEWIRE_HAS_BOUND_PROPS
            void backend_t::on_core_bound_props(void *self, uint32_t id, uint32_t global_id, const struct spa_dict *props)
            {
                lsp_trace("self=%p, id=%d, global_id=%d, props=%p",
                    self, int(id), int(global_id), props);
            }
        #endif /* PIPEWIRE_HAS_BOUND_PROPS */

            spa_thread *backend_t::on_thread_create(void *self, const spa_dict *props, void *(*start)(void*), void *arg)
            {
                backend_t * const back = cast(self);
                return ((back != NULL) && (back->pOldThreadUtils != NULL)) ?
                    spa_thread_utils_create(back->pOldThreadUtils, props, start, arg) : NULL;
            }

            int backend_t::on_thread_join(void *self, struct spa_thread *thread, void **retval)
            {
                backend_t * const back = cast(self);
                return ((back != NULL) && (back->pOldThreadUtils != NULL)) ?
                    spa_thread_utils_join(back->pOldThreadUtils, thread, retval) : -EINVAL;
            }

            int backend_t::on_thread_get_rt_range(void *self, const struct spa_dict *props, int *min, int *max)
            {
                backend_t * const back = cast(self);
                return ((back != NULL) && (back->pOldThreadUtils != NULL)) ?
                    spa_thread_utils_get_rt_range(back->pOldThreadUtils, props, min, max) : -EINVAL;
            }

            int backend_t::on_thread_acquire_rt(void *self, struct spa_thread *thread, int priority)
            {
                backend_t * const back = cast(self);
                return ((back != NULL) && (back->pOldThreadUtils != NULL)) ?
                    spa_thread_utils_acquire_rt(back->pOldThreadUtils, thread, priority) : -EINVAL;
            }

            int backend_t::on_thread_drop_rt(void *self, struct spa_thread *thread)
            {
                backend_t * const back = cast(self);
                return ((back != NULL) && (back->pOldThreadUtils != NULL)) ?
                    spa_thread_utils_drop_rt(back->pOldThreadUtils, thread) : -EINVAL;
            }

            int backend_t::on_node_transport(void *self, int readfd, int writefd, uint32_t mem_id, uint32_t offset, uint32_t size)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_set_param(void *self, uint32_t id, uint32_t flags, const spa_pod *param)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_set_io(void *self, uint32_t id, uint32_t mem_id, uint32_t offset, uint32_t size)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_event(void *self, const struct spa_event *event)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_command(void *self, const struct spa_command *command)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_add_port(void *self, spa_direction direction, uint32_t port_id, const spa_dict *props)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_remove_port(void *self, spa_direction direction, uint32_t port_id)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_port_set_param(void *self, spa_direction direction, uint32_t port_id, uint32_t id, uint32_t flags, const spa_pod *param)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_port_use_buffers(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t flags, uint32_t n_buffers, pw_client_node_buffer *buffers)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_port_set_io(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t id, uint32_t mem_id, uint32_t offset, uint32_t size)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_set_activation(void *self, uint32_t node_id, int signalfd, uint32_t mem_id, uint32_t offset, uint32_t size)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_port_set_mix_info(void *self, spa_direction direction, uint32_t port_id, uint32_t mix_id, uint32_t peer_id, const spa_dict *props)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            void backend_t::on_node_destroy(void *self)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_node_bound(void *self, uint32_t global_id)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_node_removed(void *self)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_node_done(void *self, int seq)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_node_error(void *self, int seq, int res, const char *message)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_node_bound_props(void *self, uint32_t global_id, const struct spa_dict *props)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_notify_event(void *self, uint64_t count)
            {
                lsp_trace("self = %p", self);
            }

            int backend_t::execute_context_properties_match(void *self, const char *location, const char *action, const char *val, size_t len)
            {
                lsp_trace("self = %p", self);

                backend_t * const back = cast(self);
                if ((back == NULL) || (location == NULL) || (action == NULL) || (val == NULL))
                    return -EINVAL;

                if (strcmp(action, "update-props") == 0)
                {
                    // Update properties
                    pw_properties * const props = back->sClientDict.make_properties();
                    if (props == NULL)
                        return -ENOMEM;
                    lsp_finally { pw_properties_free(props); };

                    const int res = pw_properties_update_string(props, val, len);
                    if (res < 0)
                        return res;

                    const status_t xres = back->sClientDict.set(props);
                    if (xres != STATUS_OK)
                        return -ENOMEM;
                }
                return 1;
            }

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */




