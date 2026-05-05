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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/atomic.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/common/status.h>

#include <lsp-plug.in/audio/pipewire/backend.h>
#include <lsp-plug.in/audio/pipewire/pod.h>
#include <lsp-plug.in/audio/pipewire/impl/cast.h>
#include <lsp-plug.in/audio/pipewire/impl/memmap.h>
#include <lsp-plug.in/audio/pipewire/impl/mutex.h>
#include <lsp-plug.in/audio/pipewire/impl/pw-defs.h>

#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/stdlib/stdlib.h>
#include <lsp-plug.in/stdlib/string.h>

#include <pipewire/thread.h>
#include <spa/node/io.h>
#include <spa/param/latency-utils.h>
#include <spa/support/thread.h>

#include <errno.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            static constexpr size_t NOTIFY_BUFFER_SIZE          = 1 << 16;
//            static constexpr size_t NOTIFY_BUFFER_MASK          = NOTIFY_BUFFER_SIZE - 1;

            static constexpr uint32_t PORT_TYPE_FREE            = 0xffffffff;
            static constexpr uint32_t PORT_MASK_ALL             = PORT_DIR_MASK | PORT_TYPE_MASK;

            // PipeWire core events
            const pw_core_events backend_t::core_events = {
                .version = PW_VERSION_CORE_EVENTS,
                .info = on_core_info,
                .done = on_core_done,
                .ping = NULL, // on_core_ping,
                .error = on_core_error,
                .remove_id = NULL, // on_core_remove_id,
                .bound_id = NULL, // on_core_bound_id,
                .add_mem = NULL, // on_core_add_mem,
                .remove_mem = NULL, // on_core_remove_mem,
            #ifdef PIPEWIRE_HAS_BOUND_PROPS
                .bound_props = NULL, // on_core_bound_props,
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

            // PipeWire filter events
            const pw_filter_events backend_t::filter_events =
            {
            #define PW_VERSION_FILTER_EVENTS    1
                .version            = PW_VERSION_FILTER_EVENTS,
                .destroy            = on_filter_destroy,
                .state_changed      = on_filter_state_changed,
                .io_changed         = on_filter_io_changed,
                .param_changed      = on_filter_param_changed,
                .add_buffer         = on_filter_add_buffer,
                .remove_buffer      = on_filter_remove_buffer,
                .process            = on_filter_process,
                .drained            = on_filter_drained,
                .command            = on_filter_command,
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

            // Backend implementation
            backend_t::backend_t()
            {
                construct();
            }

            void backend_t::construct()
            {
                sClientName                     = NULL;
                sServerName                     = NULL;
                pMutex                          = NULL;
                pAudioDataLoop                  = NULL;
                pContextThreadLoop              = NULL;
                pAudioLoop                      = NULL;
                pContextLoop                    = NULL;
                pContext                        = NULL;
                pCore                           = NULL;
                pMemPool                        = NULL;
                pRegistry                       = NULL;
                pFilter                         = NULL;
                pOldThreadUtils                 = NULL;

                sRegistry.construct();
                sClientDict.construct();
                sContextDict.construct();
                bzero(&sThreadUtils, sizeof(sThreadUtils));
                bzero(&sNodeInfo, sizeof(sNodeInfo));
                bzero(vHooks, sizeof(spa_hook) * HOOK_TOTAL);
                vPortMap[SPA_DIRECTION_INPUT].construct();
                vPortMap[SPA_DIRECTION_OUTPUT].construct();
                mmPosition.construct();
                mmClock.construct();

                pUserData                       = NULL;
                pCallbacks                      = NULL;
                vPorts                          = NULL;
                nPortFirst                      = 0;
                nPortCapacity                   = 0;

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
                nSyncRequestId                  = -EINVAL;
                nSyncResponseId                 = -EINVAL;
                nSyncError                      = 0;
                bActivated                      = false;

                // Export virtual table
                #define AUDIO_PIPEWIRE_BACKEND_EXP(func)    audio::backend_t::func = backend_t::func;

                AUDIO_PIPEWIRE_BACKEND_EXP(connect);
                AUDIO_PIPEWIRE_BACKEND_EXP(set_latency);
                AUDIO_PIPEWIRE_BACKEND_EXP(disconnect);
                AUDIO_PIPEWIRE_BACKEND_EXP(destroy);

                AUDIO_PIPEWIRE_BACKEND_EXP(register_port);
                AUDIO_PIPEWIRE_BACKEND_EXP(unregister_port);
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

            status_t backend_t::make_connection(
                const connection_params_t *params,
                const callbacks_t *callbacks,
                void *user_data)
            {
                int error;
                status_t res;

                if (params->client_name == NULL)
                    return STATUS_BAD_ARGUMENTS;

                // Reset parameters
                pUserData           = user_data;
                pCallbacks          = callbacks;
                nLatency            = 0;
                nSyncRequestId      = -EINVAL;
                nSyncResponseId     = -EINVAL;
                nSyncError          = 0;
                bActivated          = false;

                mmPosition.construct();
                mmClock.construct();

                // Fill client name
                sClientName             = strdup(params->client_name);
                if (sClientName == NULL)
                {
                    lsp_warn("Failed to allocate client name string");
                    return STATUS_NO_MEM;
                }

                // Fill server name
                if ((params->url != NULL) &&
                    ((strlen(params->url) == 0) ||
                    (strcmp(params->url, "default") != 0)))
                {
                    sServerName         = strdup(params->url);
                    if (sServerName == NULL)
                    {
                        lsp_warn("Failed to allocate server name string");
                        return STATUS_NO_MEM;
                    }
                }

                // Create mutex
                pMutex              = mutex_create();
                if (pMutex == NULL)
                    return STATUS_NO_MEM;

                // Create context properties
                res = sClientDict.put(
                    PW_KEY_LOOP_CANCEL, prop_false,
                    SPA_KEY_THREAD_RESET_ON_FORK, prop_false,
                    PW_KEY_REMOTE_NAME, sServerName,
                    PW_KEY_CLIENT_NAME, sClientName,
                    PW_KEY_CLIENT_API, "native",
                    PW_KEY_CONFIG_NAME, "client.conf",
                    PW_KEY_MEDIA_TYPE, BACKEND_MEDIA_TYPE,
                    PW_KEY_MEDIA_CATEGORY, BACKEND_MEDIA_CATEGORY,
                    PW_KEY_MEDIA_ROLE, BACKEND_MEDIA_ROLE);
                if (res != STATUS_OK)
                {
                    lsp_warn("Failed to initialize client properties, code=%d", int(res));
                    return res;
                }

                // Create context thread loop
                pContextThreadLoop  = pw_thread_loop_new(sClientName, NULL);
                if (pContextThreadLoop == NULL)
                {
                    lsp_warn("Failed to create PipeWire context thread loop");
                    return STATUS_UNKNOWN_ERR;
                }
                pContextLoop = pw_thread_loop_get_loop(pContextThreadLoop);

                // Create context
                {
                    lsp_trace("Context properties:\n%s\n", sClientDict.to_string());

                    pw_properties * const context_props = sClientDict.make_properties();
                    if (context_props == NULL)
                    {
                        lsp_warn("Failed to create context properties");
                        return STATUS_NO_MEM;
                    }
                    pContext            = pw_context_new(pContextLoop, context_props, 0);
                    if (pContext == NULL)
                    {
                        lsp_warn("Failed to create PipeWire context");
                        pw_properties_free(context_props);
                        return STATUS_NO_MEM;
                    }
                }

                // Update client properties
                {
                    pw_properties * const client_props = sClientDict.make_properties();
                    if (client_props == NULL)
                    {
                        lsp_warn("Failed to allocate client properties");
                        return STATUS_NO_MEM;
                    }
                    lsp_finally { pw_properties_free(client_props); };

                    error = pw_context_conf_update_props(pContext, "filter.properties", client_props);
                    if (error < 0)
                    {
                        lsp_warn("Failed to fetch context properties: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    res = sClientDict.put(client_props);
                    if (res != STATUS_OK)
                    {
                        lsp_warn("Failed to synchronize client dictionary, code=%d", int(res));
                        return res;
                    }

                    lsp_trace("Client dictionary:\n%s\n", sClientDict.to_string());
                }

                // Fetch context properties
                {
                    const pw_properties * const context_props = pw_context_get_properties(pContext);
                    if (context_props == NULL)
                    {
                        lsp_warn("Failed to obtain context properties");
                        return STATUS_UNKNOWN_ERR;
                    }

                    res = sContextDict.put(context_props);
                    if (res != STATUS_OK)
                    {
                        lsp_warn("Failed to synchronize context dictionary, code=%d", int(res));
                        return res;
                    }

                    pw_context_conf_section_match_rules(
                        pContext, "client.rules",
                        sContextDict.dict(), execute_context_properties_match, this);

                    lsp_trace("Context dictionary:\n%s", sContextDict.to_string());

                    // Update I/O parameters
                    io_parameters_t * const io = &sIOParams;
                    io->buffer_size         = pw_properties_get_uint32(context_props, "default.clock.quantum", 1024);
                    io->max_buffer_size     = pw_properties_get_uint32(context_props, "default.clock.quantum-limit", 8192);
                    io->sample_rate         = pw_properties_get_uint32(context_props, "default.clock.rate", 48000);
                }

                // Thread utils interface
                pOldThreadUtils   = static_cast<spa_thread_utils *>(pw_context_get_object(pContext, SPA_TYPE_INTERFACE_ThreadUtils));
                if (pOldThreadUtils == NULL)
                    pOldThreadUtils = pw_thread_utils_get();

                sThreadUtils      = spa_thread_utils {
                    SPA_INTERFACE_INIT(
                        SPA_TYPE_INTERFACE_ThreadUtils,
                        SPA_VERSION_THREAD_UTILS,
                        &thread_utils_impl, this)
                };

                pw_context_set_object(
                    pContext,
                    SPA_TYPE_INTERFACE_ThreadUtils,
                    &sThreadUtils);

                // Stop audio data loop
                pAudioDataLoop    = pw_context_get_data_loop(pContext);
                pAudioLoop        = pw_data_loop_get_loop(pAudioDataLoop);
                pw_data_loop_stop(pAudioDataLoop);

                // Start context thread loop
                error = pw_thread_loop_start(pContextThreadLoop);
                if (error < 0)
                {
                    lsp_warn("Failed to start context thread loop: code=%d", -error);
                    return STATUS_DISCONNECTED;
                }

                {
                    pw_thread_loop_lock(pContextThreadLoop);
                    lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                    // Connect to PipeWire Core and add Core listener
                    {
                        pw_properties * const core_properties = sClientDict.make_properties();
                        if (core_properties == NULL)
                            return STATUS_NO_MEM;
                        pCore             = pw_context_connect(pContext, core_properties, 0);
                        if (pCore == NULL)
                        {
                            lsp_warn("Could not connect to PipeWire");
                            pw_properties_free(core_properties);
                            return STATUS_DISCONNECTED;
                        }
                    }
                    error = pw_core_add_listener(pCore, &vHooks[HOOK_CORE], &core_events, this);
                    if (error < 0)
                    {
                        lsp_warn("Failed to add core listener: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    // Get memory pool
                    pMemPool  = pw_core_get_mempool(pCore);
                    if (pMemPool == NULL)
                    {
                        lsp_warn("Could not obtain memory pool");
                        return STATUS_DISCONNECTED;
                    }

                    // Get Registry and add Registry listener
                    pRegistry = pw_core_get_registry(pCore, PW_VERSION_REGISTRY, 0);
                    if (pRegistry == NULL)
                    {
                        lsp_warn("Failed to obtain PipeWire registry");
                        return STATUS_DISCONNECTED;
                    }
                    error = pw_registry_add_listener(pRegistry, &vHooks[HOOK_REGISTRY], &registry_events, this);
                    if (error < 0)
                    {
                        lsp_warn("Failed to add registry listener: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }

                    // Setup properties
                    {
                        char node_latency[16];
                        char node_rate[16];
                        const char *key_node_group = sContextDict.value(
                            PW_KEY_NODE_GROUP,
                            sClientDict.value(
                                PW_KEY_NODE_GROUP, "group.dsp.0"));

                        snprintf(node_latency, sizeof(node_latency), "0/%d", int(sIOParams.sample_rate));
                        snprintf(node_rate, sizeof(node_rate), "1/%d", int(sIOParams.sample_rate));

                        res = sClientDict.put(
                            PW_KEY_NODE_NAME, sClientName,
                            PW_KEY_NODE_GROUP, key_node_group,
                            PW_KEY_NODE_DESCRIPTION, sClientName,
                            PW_KEY_MEDIA_TYPE, "Audio",
                            PW_KEY_MEDIA_CATEGORY, "Duplex",
                            PW_KEY_MEDIA_ROLE, "DSP",
                            PW_KEY_NODE_LATENCY, node_latency,
                            PW_KEY_NODE_RATE, node_rate,
                            PW_KEY_NODE_ALWAYS_PROCESS, prop_true,
                            PW_KEY_NODE_TRANSPORT_SYNC, prop_true);
                        if (res != STATUS_OK)
                        {
                            lsp_warn("Failed to synchronize client dictionary, code=%d", int(res));
                            return res;
                        }

                        lsp_trace("Node dictionary:\n%s", sClientDict.to_string());
                    }

                    // Create PipeWire filter
                    {
                        pw_properties * const filter_props = sClientDict.make_properties();
                        pFilter = pw_filter_new(pCore, sClientName, filter_props);

                        if (pFilter == NULL)
                        {
                            lsp_warn("Could not create PipeWire filter");
                            return STATUS_DISCONNECTED;
                        }

                        pw_filter_add_listener(pFilter, &vHooks[HOOK_FILTER], &filter_events, this);
                    }

                    // Issue sync request
                    error = sync_core(false);
                    if (error < 0)
                    {
                        lsp_warn("Failed to synchrnonize with PipeWire server: code=%d", -error);
                        return STATUS_DISCONNECTED;
                    }
                }

                return STATUS_OK;
            }

            void backend_t::close_connection()
            {
                if (pContextThreadLoop != NULL)
                {
                    // Destroy all related objects
                    {
                        pw_thread_loop_lock(pContextThreadLoop);
                        lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                        // Destroy node
                        if (pFilter != NULL)
                        {
                            spa_hook_remove(&vHooks[HOOK_FILTER]);
                            pw_filter_destroy(pFilter);
                            pFilter     = NULL;
                        }

                        // Destroy registry
                        if (pRegistry != NULL)
                        {
                            spa_hook_remove(&vHooks[HOOK_REGISTRY]);
                            pw_proxy_destroy(to_pw_proxy(pRegistry));
                            pRegistry   = NULL;
                        }

                        // Release memory mappings
                        mmPosition.free();
                        mmClock.free();

                        // Destroy core
                        if (pCore != NULL)
                        {
                            spa_hook_remove(&vHooks[HOOK_CORE]);
                            pw_core_disconnect(pCore);
                            pCore     = NULL;
                        }
                        pMemPool  = NULL;

                        // Destroy context
                        if (pContext != NULL)
                        {
                            pw_context_destroy(pContext);
                            pContext = NULL;
                        }
                    }

                    pw_thread_loop_stop(pContextThreadLoop);
                }

                pAudioLoop = NULL;
                pAudioDataLoop = NULL;

                if (pContextThreadLoop != NULL)
                {
                    pw_thread_loop_destroy(pContextThreadLoop);
                    pContextThreadLoop = NULL;
                    pContextLoop = NULL;
                }

                if (pMutex != NULL)
                {
                    mutex_destroy(pMutex);
                    pMutex      = NULL;
                }

                if (sServerName != NULL)
                {
                    free(sServerName);
                    sServerName = NULL;
                }

                if (sClientName != NULL)
                {
                    free(sClientName);
                    sClientName = NULL;
                }

                sRegistry.destroy();
                sClientDict.destroy();
                sContextDict.destroy();
                bzero(&sThreadUtils, sizeof(sThreadUtils));
                bzero(&sNodeInfo, sizeof(sNodeInfo));
                bzero(vHooks, sizeof(spa_hook) * HOOK_TOTAL);

                pCallbacks      = NULL;
                pUserData       = NULL;
            }

            status_t backend_t::connect(
                audio::backend_t *self,
                const connection_params_t *params,
                const callbacks_t *callbacks,
                void *user_data)
            {
                backend_t * const back = cast(self);

                // Check that backend is disconnected
                if (back->pFilter != NULL)
                    return STATUS_BAD_STATE;

                // Set-up destruction hook
                lsp_finally {
                    if (!back->bActivated)
                        back->close_connection();
                };

                // Make connection
                status_t res        = back->make_connection(params, callbacks, user_data);
                if (res != STATUS_OK)
                    return res;

                // Register already attached ports
                res                 = back->register_ports();
                if (res != STATUS_OK)
                {
                    back->unregister_ports();
                    return res;
                }

                // Issue connected callback
                res = ((callbacks) && (callbacks->on_connected)) ?
                    callbacks->on_connected(user_data, &back->sIOParams) :
                    STATUS_OK;
                lsp_finally {
                    if ((!back->bActivated) && (callbacks) && (callbacks->on_connection_lost))
                        callbacks->on_connection_lost(user_data);
                };
                if (res != STATUS_OK)
                    return res;

                // Issue activation callback
                res = ((callbacks) && (callbacks->on_activated)) ?
                    callbacks->on_activated(user_data) :
                    STATUS_OK;
                if (res != STATUS_OK)
                    return res;

                // Activate the client
                res                 = back->activate();
                if (res != STATUS_OK)
                {
                    lsp_error("Could not activate PipeWire client");

                    // Issue deactivation callback
                    if ((callbacks) && (callbacks->on_deactivated))
                        callbacks->on_deactivated(user_data);

                    return res;
                }

                return STATUS_OK;
            }

            status_t backend_t::disconnect(audio::backend_t *self)
            {
                if (self == NULL)
                    return STATUS_BAD_ARGUMENTS;

                // Ensure that we are connected
                backend_t * const back = cast(self);
                if (back->pFilter == NULL)
                    return STATUS_BAD_STATE;

                // Get callbacks table and user data
                const callbacks_t * const cb    = back->pCallbacks;
                void * const user_data          = back->pUserData;
                status_t res                    = STATUS_OK;

                // Deactivate plugin
                if (back->bActivated)
                {
                    // Deactivate client
                    res                     = update_status(res, back->deactivate());

                    // Issue deactivation callback
                    if ((cb) && (cb->on_deactivated))
                        cb->on_deactivated(user_data);
                }

                // Unregister ports
                back->unregister_ports();

                // Close client connection
                back->close_connection();

                if ((cb) && (cb->on_disconnected))
                    cb->on_disconnected(user_data);

                return res;
            }

            status_t backend_t::activate()
            {
                // Lock the loop
                pw_thread_loop_lock(pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                // Start audio data loop
                int error = pw_data_loop_start(pAudioDataLoop);
                if (error < 0)
                {
                    lsp_error("Failed to launch PipeWire data loop: error=%d", int(error));
                    return STATUS_DISCONNECTED;
                }
                lsp_finally {
                    if (!bActivated)
                        pw_data_loop_stop(pAudioDataLoop);
                };

                /* Now connect this filter. We ask that our process function is
                 * called in a realtime thread. */
                error = pw_filter_connect(
                    pFilter,
                    PW_FILTER_FLAG_RT_PROCESS,
                    NULL, 0);
                if (error < 0)
                {
                    lsp_error("Failed to connect PipeWire filter: error=%d", int(error));
                    return STATUS_DISCONNECTED;
                }

                // Set node active
                error = pw_filter_set_active(pFilter, true);
                if (error < 0)
                {
                    lsp_error("Failed to activate PipeWire filter: error=%d", int(error));
                    return STATUS_DISCONNECTED;
                }
                lsp_finally {
                    if (!bActivated)
                        pw_data_loop_stop(pAudioDataLoop);
                };

                // Synchronize core
                error = sync_core(false);
                if (error < 0)
                {
                    lsp_error("Failed to synchronize with PipeWire core: error=%d", int(error));
                    return STATUS_DISCONNECTED;
                }

                // Mark backend activated
                bActivated      = true;

                return STATUS_OK;
            }

            status_t backend_t::deactivate()
            {
                if (!bActivated)
                    return STATUS_OK;

                // Lock the loop
                pw_thread_loop_lock(pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                // Stop audio data loop
                status_t res = STATUS_OK;
                int error = pw_data_loop_stop(pAudioDataLoop);
                if (error < 0)
                {
                    lsp_error("Failed to stop PipeWire data loop: error=%d", int(error));
                    res         = STATUS_UNKNOWN_ERR;
                }

                // Mark node as inactive
                error = pw_filter_set_active(pFilter, false);
                if (error < 0)
                {
                    lsp_error("Failed to deactivate PipeWire filter: error=%d", int(error));
                    res         = STATUS_UNKNOWN_ERR;
                }

                // Disconnect the filter
                error = pw_filter_disconnect(pFilter);
                if (error < 0)
                {
                    lsp_error("Failed to disconnect PipeWire filter: error=%d", int(error));
                    res         = STATUS_UNKNOWN_ERR;
                }

                // Synchronize core
                error = sync_core(false);
                if (error < 0)
                {
                    lsp_error("Failed to synchronize with PipeWire core: error=%d", int(error));
                    res         = STATUS_UNKNOWN_ERR;
                }

                bActivated        = false;

                return res;
            }

            int backend_t::sync_core(bool lock)
            {
                if (pCore == NULL)
                    return -EINVAL;

                if (pw_thread_loop_in_thread(pContextThreadLoop))
                {
                    lsp_warn("sync requested from callback");
                    return 0;
                }

                if (lock)
                    pw_thread_loop_lock(pContextThreadLoop);
                lsp_finally {
                    if (lock)
                        pw_thread_loop_unlock(pContextThreadLoop);
                };

                if (nSyncError == -EPIPE)
                    return nSyncError;

                nSyncError      = 0;
                nSyncResponseId = -EINVAL;
                nSyncRequestId  = pw_proxy_sync(to_pw_proxy(pCore), 0);

                do
                {
                    pw_thread_loop_wait(pContextThreadLoop);
                } while ((nSyncResponseId >= 0) && (nSyncRequestId != nSyncResponseId));

                if (nSyncResponseId < 0)
                    nSyncError      = nSyncResponseId;

                return nSyncResponseId;
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

                // Free allocated ports
                back->vPortMap[SPA_DIRECTION_INPUT].destroy();
                back->vPortMap[SPA_DIRECTION_OUTPUT].destroy();
                if (back->vPorts != NULL)
                {
                    for (port_id_t i=0; i<back->nPortCapacity; ++i)
                        back->free_port(&back->vPorts[i]);

                    free(back->vPorts);
                    back->vPorts        = NULL;
                }

                // Deallocate memory
                free(back);
            }

            status_t backend_t::register_port(port_t *port)
            {
                // Allocate port global name
                const size_t name_bytes = strlen(sClientName) + 1;
                const size_t id_bytes   = strlen(port->sID) + 1;
                char *full_id           = malloc_bytes<char>(name_bytes + id_bytes);
                if (full_id == NULL)
                    return STATUS_NO_MEM;
                lsp_finally {
                    if (full_id != NULL)
                        free(full_id);
                };
                memcpy(full_id, sClientName, name_bytes - 1);
                full_id[name_bytes-1] = ':';
                memcpy(&full_id[name_bytes], port->sID, id_bytes);

                // Initialize dictionary
                dictionary dict;
                status_t res = dict.put(
                    PW_KEY_PORT_NAME, port->sID,
                    PW_KEY_FORMAT_DSP, port_format_dsp(port->nType),
                    PW_KEY_PORT_PHYSICAL, prop_false,
                    PW_KEY_PORT_TERMINAL, prop_false);
                if (res != STATUS_OK)
                {
                    lsp_warn("Failed to initialize properties for port id=%s", port->sID);
                    return -res;
                }

                // Setup port pointers
                pw_properties * const port_props = dict.make_properties();
                if (port_props == NULL)
                {
                    lsp_warn("Failed to allocate port properties for port id=%s", port->sID);
                    return STATUS_NO_MEM;
                }

                const spa_direction direction   = ((port->nType & PORT_DIR_MASK) == PORT_DIR_IN) ?
                    SPA_DIRECTION_INPUT : SPA_DIRECTION_OUTPUT;

                port->pHandle = pw_filter_add_port(
                    pFilter,
                    direction,
                    PW_FILTER_PORT_FLAG_MAP_BUFFERS,
                    0,
                    port_props,
                    NULL, 0);

                // Port has been successfully registered
                port->sFullId               = release_ptr(full_id);

                return STATUS_OK;
            }

            status_t backend_t::unregister_port(port_t *port)
            {
                // Check that port has been registered
                if (port->sFullId == NULL)
                    return STATUS_OK;
                free(port->sFullId);
                port->sFullId       = NULL;
                port->pHandle       = NULL;

//                if (port->pHandle != NULL)
//                {
//                    int error = pw_filter_remove_port(port->pHandle);
//                    if (error < 0)
//                    {
//                        lsp_trace("Error removing node port %s, error=%d", port->sID, int(-error));
//                        return STATUS_UNKNOWN_ERR;
//                    }
//                }

                return STATUS_OK;
            }

            status_t backend_t::register_ports()
            {
                // Register all ports
                status_t res;
                for (port_id_t i=0, n=nPortCapacity; i<n; ++i)
                {
                    port_t * const port     = &vPorts[i];
                    if (port->nType == PORT_TYPE_FREE)
                        continue;

                    res                     = register_port(port);
                    if (res != STATUS_OK)
                        return res;
                }

                // Synchronize
                int error = sync_core(true);
                if (error < 0)
                {
                    lsp_trace("Error performing client sync, error=%d", int(error));
                    return STATUS_UNKNOWN_ERR;
                }

                return STATUS_OK;
            }

            void backend_t::unregister_ports()
            {
                // Unregister all ports
                for (port_id_t i=0, n=nPortCapacity; i<n; ++i)
                {
                    port_t * const port     = &vPorts[i];
                    if (port->nType != PORT_TYPE_FREE)
                        unregister_port(port);
                }

                // Synchronize client
                int error   = sync_core(true);
                if (error < 0)
                    lsp_warn("Error synchronizing with client, code=%d", int(-error));
            }

            port_id_t backend_t::register_port(audio::backend_t *self, const char *id, uint32_t flags)
            {
                // Check arguments
                if (id == NULL)
                    return -STATUS_INVALID_VALUE;
                // Check arguments
                const size_t id_len = strlen(id);
                if (id_len <= 0)
                    return -STATUS_INVALID_VALUE;
                else if (id_len >= MAX_PORT_ID_BYTES)
                    return -STATUS_TOO_BIG;

                backend_t * const back      = cast(self);

                // Check for duplicates
                port_t * port               = back->find_port(id);
                if (port != NULL)
                    return -STATUS_ALREADY_EXISTS;

                // Allocate port
                port                        = back->alloc_port(id, flags);
                if (port == NULL)
                    return -STATUS_NO_MEM;
                lsp_finally { back->free_port(port); };

                // Need to immediately register port (connected)?
                if (back->pFilter != NULL)
                {
                    // Register port
                    status_t res                = back->register_port(port);
                    if (res != STATUS_OK)
                        return -res;

                    // Synchronize client
                    int error   = back->sync_core(true);
                    if (error < 0)
                    {
                        back->unregister_port(port);
                        lsp_error("Error synchronizing with client, code=%d", int(-error));
                        return -STATUS_UNKNOWN_ERR;
                    }
                }

                return release_ptr(port) - back->vPorts;
            }

            status_t backend_t::unregister_port(audio::backend_t *self, port_id_t port_id)
            {
                // Check port identifier
                backend_t * const back      = cast(self);
                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return STATUS_INVALID_VALUE;

                // Check port state
                port_t * const port         = &back->vPorts[port_id];
                if (port->nType == PORT_TYPE_FREE)
                    return STATUS_INVALID_VALUE;

                // Need to immediately unregister port?
                if (back->pFilter != NULL)
                {
                    // Register port
                    status_t res                = back->unregister_port(port);
                    if (res != STATUS_OK)
                        return -res;

                    // Synchronize client
                    int error   = back->sync_core(true);
                    if (error < 0)
                    {
                        back->unregister_port(port);
                        lsp_error("Error synchronizing with client, code=%d", int(-error));
                        return -STATUS_UNKNOWN_ERR;
                    }
                }

                // Deallocate port
                back->free_port(port);

                return STATUS_OK;
            }

            const char *backend_t::port_system_name(audio::backend_t *self, port_id_t port_id)
            {
                backend_t * const back      = cast(self);

                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return NULL;

                port_t * const port         = &back->vPorts[port_id];
                if (port->nType == PORT_TYPE_FREE)
                    return NULL;

                return port->sFullId;
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
                backend_t * const back      = cast(self);
                lsp_trace("self=%p, id=%d, permissions=0x%x, type:%s/%d, props=%p\n",
                    self, int(id), int(permissions), type, int(version), props);

            #ifdef LSP_TRACE
                dictionary dict;
                if ((props != NULL) && (dict.set(props) == STATUS_OK))
                    lsp_trace("Related properties:\n%s\n", dict.to_string());
            #endif /* LSP_TRACE */

                MUTEX_SCOPED_LOCK(back->pMutex);

                // Check that we need to synchronize node name
                bool sync_node_name = false;
                if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0)
                {
                    if ((back->pFilter != NULL) && (id == pw_filter_get_node_id(back->pFilter)))
                        sync_node_name  = back->sRegistry.find_node_by_name(back->sClientName) != NULL;
                }

                const status_t res = back->sRegistry.process_add(id, permissions, type, version, props);
                if (res == STATUS_OK)
                {
                    const node_t * const node = (sync_node_name) ?
                        back->sRegistry.find_node_by_id(pw_filter_get_node_id(back->pFilter)) : NULL;
                    if (node != NULL)
                    {
                        char * new_name = strdup(node->sUID);
                        if (new_name != NULL)
                        {
                            lsp_trace("Node name updated from '%s' to '%s'", back->sClientName, new_name);
                            free(back->sClientName);
                            back->sClientName = new_name;
                        }
                    }
                }
                else
                {
                    lsp_warn("Error while processing registry add event "
                        "self=%p, id=%d, permissions=0x%x, type:%s/%d, props=%p: code=%d",
                        self, int(id), int(permissions), type, int(version), props, int(res));
                }
            }

            void backend_t::on_registry_event_removed(void *self, uint32_t id)
            {
                lsp_trace("self=%p, id=%d\n", self, int(id));
                backend_t * const back      = cast(self);

                MUTEX_SCOPED_LOCK(back->pMutex);

                status_t res = back->sRegistry.process_remove(id);
                if (res != STATUS_OK)
                {
                    lsp_warn("Error while processing registry remove event "
                        "self=%p, id=%d",
                        self, int(id));
                }
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

            #ifdef LSP_TRACE
                dictionary dict;
                if ((info->props) && (dict.set(info->props) == STATUS_OK))
                    lsp_trace("Core info properties:\n%s\n", dict.to_string());
            #endif /* LSP_TRACE */
            }

            void backend_t::on_core_done(void *self, uint32_t id, int seq)
            {
                lsp_trace("self=%p, id=%d, seq=%d", self, int(id), int(seq));

                backend_t * const back = cast(self);
                if (id != PW_ID_CORE)
                    return;

                back->nSyncResponseId   = seq;
                if ((back->nSyncRequestId == seq) || (back->nSyncResponseId < 0))
                    pw_thread_loop_signal(back->pContextThreadLoop, false);
            }

            void backend_t::on_core_ping(void *self, uint32_t id, int seq)
            {
                lsp_trace("self=%p, id=%d, seq=%d", self, int(id), int(seq));
                backend_t * const back = cast(self);
                if (back->pCore != NULL)
                    pw_core_pong(back->pCore, id, seq);
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

        #ifdef LSP_TRACE
            dictionary dict;
            if ((props) && (dict.set(props) == STATUS_OK))
                lsp_trace("Core bound properties:\n%s\n", dict.to_string());
        #endif /* LSP_TRACE */
            }
        #endif /* PIPEWIRE_HAS_BOUND_PROPS */

            spa_thread *backend_t::on_thread_create(void *self, const spa_dict *props, void *(*start)(void*), void *arg)
            {
                backend_t * const back = cast(self);
                return (back->pOldThreadUtils != NULL) ?
                    spa_thread_utils_create(back->pOldThreadUtils, props, start, arg) : NULL;
            }

            int backend_t::on_thread_join(void *self, struct spa_thread *thread, void **retval)
            {
                backend_t * const back = cast(self);
                return (back->pOldThreadUtils != NULL) ?
                    spa_thread_utils_join(back->pOldThreadUtils, thread, retval) : -EINVAL;
            }

            int backend_t::on_thread_get_rt_range(void *self, const struct spa_dict *props, int *min, int *max)
            {
                backend_t * const back = cast(self);
                return (back->pOldThreadUtils != NULL) ?
                    spa_thread_utils_get_rt_range(back->pOldThreadUtils, props, min, max) : -EINVAL;
            }

            int backend_t::on_thread_acquire_rt(void *self, struct spa_thread *thread, int priority)
            {
                backend_t * const back = cast(self);
                return (back->pOldThreadUtils != NULL) ?
                    spa_thread_utils_acquire_rt(back->pOldThreadUtils, thread, priority) : -EINVAL;
            }

            int backend_t::on_thread_drop_rt(void *self, struct spa_thread *thread)
            {
                backend_t * const back = cast(self);
                return (back->pOldThreadUtils != NULL) ?
                    spa_thread_utils_drop_rt(back->pOldThreadUtils, thread) : -EINVAL;
            }

            void backend_t::on_filter_destroy(void *self)
            {
            }

            void backend_t::on_filter_state_changed(void *self, enum pw_filter_state old, enum pw_filter_state state, const char *error)
            {
            }

            void backend_t::on_filter_io_changed(void *self, void *port_data, uint32_t id, void *area, uint32_t size)
            {
            }

            void backend_t::on_filter_param_changed(void *self, void *port_data, uint32_t id, const struct spa_pod *param)
            {
            }

            void backend_t::on_filter_add_buffer(void *self, void *port_data, struct pw_buffer *buffer)
            {
            }

            void backend_t::on_filter_remove_buffer(void *self, void *port_data, struct pw_buffer *buffer)
            {
            }

            void backend_t::on_filter_process(void *self, struct spa_io_position *position)
            {
            }

            void backend_t::on_filter_drained(void *self)
            {
            }

            void backend_t::on_filter_command(void *self, const struct spa_command *command)
            {
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
            #ifdef LSP_TRACE
                const char *id_desc = "unknown";
                #define DECODE(value) \
                    case value: id_desc=LSP_STRINGIFY(value); break;
                switch (id)
                {
                    DECODE(SPA_IO_Invalid)
                    DECODE(SPA_IO_Buffers)
                    DECODE(SPA_IO_Range)
                    DECODE(SPA_IO_Clock)
                    DECODE(SPA_IO_Latency)
                    DECODE(SPA_IO_Control)
                    DECODE(SPA_IO_Notify)
                    DECODE(SPA_IO_Position)
                    DECODE(SPA_IO_RateMatch)
                    DECODE(SPA_IO_Memory)
                    DECODE(SPA_IO_AsyncBuffers)
                    default: break;
                }
                #undef DECODE
                lsp_trace("self = %p, id=%d (%s), mem_id=%d, offset=%d, size=%d",
                    self, int(id), id_desc, int(mem_id), int(offset), int(size));
            #endif /* LSP_TRACE */

                backend_t * const back = cast(self);
                switch (id)
                {
                    case SPA_IO_Clock:
                        back->mmClock.remap(back, mem_id, offset, size);
                        break;
                    case SPA_IO_Position:
                        back->mmPosition.remap(back, mem_id, offset, size);
                        break;
                    default:
                        break;
                }

                return 0;
            }

            int backend_t::on_node_event(void *self, const struct spa_event *event)
            {
                lsp_trace("self = %p", self);
                return 0;
            }

            int backend_t::on_node_command(void *self, const struct spa_command *command)
            {
                lsp_trace("self = %p, command=%d (%s)",
                    self,
                    int(command->body.body.type),
                    decode_spa_command_type(command->body.body.type));

                if (SPA_COMMAND_TYPE(command) == SPA_TYPE_COMMAND_Node)
                {
                    lsp_trace("Node command: id=%d (%s)",
                        int(command->body.body.id),
                        decode_spa_node_command(command->body.body.id));

                }

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
//            #ifdef LSP_TRACE
//                backend_t * const back = cast(self);
//                lsp_trace("Activation self=%p (%s), node_id=%d, signalfd=%d, mem_id=%d, offset=%d, size=%d",
//                    self,
//                    (back->nNodeGlobalId == node_id) ? "our" : "set",
//                    int(node_id), int(signalfd), int(mem_id), int(offset), int(size));
//            #endif /* LSP_TRACE */

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
                backend_t * const back  = cast(self);
                lsp_trace("Node '%s' bound with global_id=%d", back->sClientName, int(global_id));
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

            #ifdef LSP_TRACE
                dictionary dict;
                if ((props) && (dict.set(props) == STATUS_OK))
                    lsp_trace("Node bound properties:\n%s\n", dict.to_string());
            #endif /* LSP_TRACE */
            }

            void backend_t::on_notify_event(void *self, uint64_t count)
            {
                lsp_trace("self = %p", self);
            }

            int backend_t::execute_context_properties_match(void *self, const char *location, const char *action, const char *val, size_t len)
            {
                lsp_trace("self = %p", self);

                backend_t * const back = cast(self);

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

            backend_t::port_t *backend_t::find_port(const char *id)
            {
                const uint32_t capacity = nPortCapacity;

                // Find allocated port with the same identifier in list
                for (size_t i=0 ; i < capacity; ++i)
                {
                    port_t * const port     = &vPorts[i];
                    if ((port->nType != PORT_TYPE_FREE) &&
                        (strcmp(port->sID, id) == 0))
                        return port;
                }

                return NULL;
            }

            void backend_t::init_port(port_t *port)
            {
                port->nType             = PORT_TYPE_FREE;
                port->pHandle           = NULL;
                port->sFullId           = NULL;
                port->sID[0]            = '\0';
            }

            backend_t::port_t *backend_t::alloc_port(const char *id, uint32_t flags)
            {
                uint32_t first          = nPortFirst;
                uint32_t capacity       = nPortCapacity;

                // Check that memory should be re-allocated
                if (first >= capacity)
                {
                    const size_t new_cap    = lsp_max((capacity << 1), 4u);
                    port_t * const items    = realloc_count<port_t>(vPorts, new_cap);
                    if (!items)
                        return NULL;

                    for (size_t i=capacity; i<new_cap; ++i)
                        init_port(&items[i]);

                    capacity                = new_cap;
                    vPorts                  = items;
                    nPortCapacity           = capacity;
                }

                // Find unused port in list
                for ( ; first < capacity; ++first)
                {
                    port_t * const port     = &vPorts[first];
                    if (port->nType == PORT_TYPE_FREE)
                    {
                        port->nType             = flags & PORT_MASK_ALL;
                        port->sFullId           = NULL;
                        strncpy(port->sID, id, MAX_PORT_ID_BYTES);
                        port->sID[MAX_PORT_ID_BYTES-1]  = '\0';

                        nPortFirst              = first + 1;
                        return port;
                    }
                }

                nPortFirst              = first;

                return NULL;
            }

            void backend_t::free_port(port_t *port)
            {
                if ((port == NULL) || (port->nType == PORT_TYPE_FREE))
                    return;

                // Release previously allocated data
                if (port->sFullId != NULL)
                    free(port->sFullId);

                // Reset port state
                init_port(port);

                nPortFirst          = lsp_min(nPortFirst, port_id_t(port - vPorts));
            }

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */




