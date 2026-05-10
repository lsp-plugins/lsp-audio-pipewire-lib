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
#include <lsp-plug.in/audio/pipewire/messages.h>
#include <lsp-plug.in/audio/pipewire/pod.h>
#include <lsp-plug.in/audio/pipewire/impl/cast.h>
#include <lsp-plug.in/audio/pipewire/impl/pw-defs.h>

#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/stdlib/stdlib.h>
#include <lsp-plug.in/stdlib/string.h>

#include <pipewire/thread.h>
#include <spa/control/control.h>
#include <spa/node/io.h>
#include <spa/param/audio/raw.h>
#include <spa/param/audio/raw-utils.h>
#include <spa/param/latency-utils.h>
#include <spa/support/thread.h>
#include <spa/utils/json.h>

#include <errno.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
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

            const pw_proxy_events backend_t::metadata_proxy_events = {
                .version            = PW_VERSION_PROXY_EVENTS,
                .destroy            = on_metadata_destroy,
                .bound              = NULL,
                .removed            = on_metadata_removed,
                .done               = NULL,
                .error              = NULL,
                .bound_props        = NULL
            };

            const pw_metadata_events backend_t::metadata_events = {
                .version            = PW_VERSION_METADATA_EVENTS,
                .property           = on_metadata_property
            };

            const pw_proxy_events backend_t::link_proxy_events = {
                .version            = PW_VERSION_PROXY_EVENTS,
                .destroy            = NULL,
                .bound              = NULL,
                .removed            = NULL,
                .done               = NULL,
                .error              = on_link_error,
                .bound_props        = NULL
            };

            // Backend implementation
            backend_t::backend_t() noexcept
            {
                construct();
            }

            void backend_t::construct() noexcept
            {
                sClientName                     = NULL;
                sServerName                     = NULL;
                pAudioDataLoop                  = NULL;
                pContextThreadLoop              = NULL;
                pAudioLoop                      = NULL;
                pContextLoop                    = NULL;
                pContext                        = NULL;
                pCore                           = NULL;
                pMemPool                        = NULL;
                pRegistry                       = NULL;
                pFilter                         = NULL;
                pMetadata                       = NULL;
                pOldThreadUtils                 = NULL;

                sRegistry.construct();
                sRingBuffer.construct();
                sClientDict.construct();
                sContextDict.construct();
                bzero(&sThreadUtils, sizeof(sThreadUtils));
                bzero(vHooks, sizeof(spa_hook) * HOOK_TOTAL);

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

                AUDIO_PIPEWIRE_BACKEND_EXP(read_midi_event);
                AUDIO_PIPEWIRE_BACKEND_EXP(write_midi_event);

                #undef AUDIO_PIPEWIRE_BACKEND_EXP
            }

            status_t backend_t::make_connection(
                const connection_params_t *params,
                const callbacks_t *callbacks,
                void *user_data) noexcept
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

                // Bind ring buffer
                res = sRingBuffer.init(pContextLoop, RING_BUFFER_SIZE, on_ringbuffer_data_received, this);
                if (res != STATUS_OK)
                {
                    lsp_warn("Failed to initialize ring buffer handler, code=%d", int(res));
                    return res;
                }

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

            void backend_t::close_connection() noexcept
            {
                if (pContextThreadLoop != NULL)
                {
                    // Destroy all related objects
                    {
                        pw_thread_loop_lock(pContextThreadLoop);
                        lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

                        // Destroy metadata
                        if (pMetadata != NULL)
                        {
                            spa_hook_remove(&vHooks[HOOK_METADATA_PROXY]);
                            spa_hook_remove(&vHooks[HOOK_METADATA]);
                            pw_proxy_destroy(to_pw_proxy(pMetadata));
                            pMetadata   = NULL;
                        }

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
                sRingBuffer.destroy();
                sClientDict.destroy();
                sContextDict.destroy();
                bzero(&sThreadUtils, sizeof(sThreadUtils));
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

            status_t backend_t::activate() noexcept
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

                // Now connect this filter. We ask that our process function is called in a realtime thread
                pod_builder pod;
                uint32_t n_params = 0;
                const spa_pod *params[1];
                params[n_params++] = pod.make_audio_format_pod(sIOParams.sample_rate);

                error = pw_filter_connect(
                    pFilter,
                    PW_FILTER_FLAG_RT_PROCESS,
                    params, n_params);
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

            status_t backend_t::deactivate() noexcept
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

            int backend_t::sync_core(bool lock) noexcept
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
                backend_t * const back  = cast(self);

                // Notify the main thread about latency change
                message_t msg;
                init_header(&msg.header, MSG_LATENCY, sizeof(latency_t));
                msg.latency.latency = latency;

                return write_message(&back->sRingBuffer, &msg);
            }

            void backend_t::destroy(audio::backend_t *self)
            {
                backend_t * const back          = cast(self);

                // Issue disconnect and free allocated memory
                disconnect(back);

                // Free allocated ports
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

            char *backend_t::make_port_full_id(const char *id) const noexcept
            {
                // Allocate port global name
                const size_t name_bytes = strlen(sClientName) + 1;
                const size_t id_bytes   = strlen(id) + 1;
                char *full_id           = malloc_bytes<char>(name_bytes + id_bytes);
                if (full_id == NULL)
                    return NULL;
                memcpy(full_id, sClientName, name_bytes - 1);
                full_id[name_bytes-1] = ':';
                memcpy(&full_id[name_bytes], id, id_bytes);

                return full_id;
            }

            status_t backend_t::register_port(port_t *port) noexcept
            {
                // Allocate port global name
                char *full_id           = make_port_full_id(port->sID);
                if (full_id == NULL)
                    return STATUS_NO_MEM;
                lsp_finally {
                    if (full_id != NULL)
                        free(full_id);
                };

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

                // Add port to the filter
                port->pHandle = static_cast<port_data_t *>(
                    pw_filter_add_port(
                        pFilter,
                        direction,
                        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
                        sizeof(port_data_t),
                        port_props,
                        NULL, 0));

                if (port->pHandle == NULL)
                {
                    lsp_warn("Failed to add port id=%s", port->sID);
                    return STATUS_NO_MEM;
                }
                port->pHandle->nPortId      = port - vPorts;

                // Port has been successfully registered
                port->sFullId               = release_ptr(full_id);

                return STATUS_OK;
            }

            status_t backend_t::unregister_port(port_t *port) noexcept
            {
                // Check that port has been registered
                if (port->sFullId == NULL)
                    return STATUS_OK;
                free(port->sFullId);
                port->sFullId       = NULL;
                port->pHandle       = NULL;
                port->pBuffer       = NULL;

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

            status_t backend_t::register_ports() noexcept
            {
                // Register all ports
                {
                    pw_thread_loop_lock(pContextThreadLoop);
                    lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };

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

            void backend_t::unregister_ports() noexcept
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

                // Determine flags
                switch (flags & PORT_TYPE_MASK)
                {
                    case PORT_TYPE_AUDIO:
                    case PORT_TYPE_MIDI:
                    case PORT_TYPE_MIDI2:
                        break;
                    default:
                        return -STATUS_INVALID_VALUE;
                }

                // Allocate port
                port                        = back->alloc_port(id, flags);
                if (port == NULL)
                    return -STATUS_NO_MEM;
                lsp_finally { back->free_port(port); };

                // Need to immediately register port (connected)?
                if (back->pFilter != NULL)
                {
                    pw_thread_loop_lock(back->pContextThreadLoop);
                    lsp_finally { pw_thread_loop_unlock(back->pContextThreadLoop); };

                    // Register port
                    status_t res                = back->register_port(port);
                    if (res != STATUS_OK)
                        return -res;

                    // Synchronize client
                    int error   = back->sync_core(false);
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
                if ((source == NULL) || (destination == NULL))
                    return STATUS_BAD_ARGUMENTS;

                backend_t * const back      = cast(self);

                pw_thread_loop_lock(back->pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(back->pContextThreadLoop); };

                // Find source and destination port and their matches
                const pipewire::port_t * const src  = back->sRegistry.find_port(source, PORT_DIR_OUT);
                if (src == NULL)
                    return STATUS_NOT_FOUND;
                const pipewire::port_t * const dst  = back->sRegistry.find_port(destination, PORT_DIR_IN);
                if (dst == NULL)
                    return STATUS_NOT_FOUND;
                if ((src->nFlags & PORT_TYPE_MASK) != (dst->nFlags & PORT_TYPE_MASK))
                    return STATUS_BAD_TYPE;

                dictionary dict;
                status_t res;
                char tmp[32];

                snprintf(tmp, sizeof(tmp), "%u", (unsigned int)(src->nNodeID));
                if ((res = dict.put(PW_KEY_LINK_OUTPUT_NODE, tmp)) != STATUS_OK)
                    return res;
                snprintf(tmp, sizeof(tmp), "%u", (unsigned int)(src->nPortID));
                if ((res = dict.put(PW_KEY_LINK_OUTPUT_PORT, tmp)) != STATUS_OK)
                    return res;
                snprintf(tmp, sizeof(tmp), "%u", (unsigned int)(dst->nNodeID));
                if ((res = dict.put(PW_KEY_LINK_INPUT_NODE, tmp)) != STATUS_OK)
                    return res;
                snprintf(tmp, sizeof(tmp), "%u", (unsigned int)(dst->nPortID));
                if ((res = dict.put(PW_KEY_LINK_INPUT_PORT, tmp)) != STATUS_OK)
                    return res;
                if ((res = dict.put(PW_KEY_OBJECT_LINGER, prop_true)) != STATUS_OK)
                    return res;

                pw_proxy * const proxy = static_cast<pw_proxy *>(
                    pw_core_create_object(
                        back->pCore,
                        "link-factory",
                        PW_TYPE_INTERFACE_Link,
                        PW_VERSION_LINK,
                        dict.dict(),
                        0));
                if (proxy == NULL)
                    return STATUS_UNKNOWN_ERR;

                spa_hook listener;
                bzero(&listener, sizeof(listener));
                int link_res = 0;

                pw_proxy_add_listener(proxy, &listener, &link_proxy_events, &link_res);
                int error = back->sync_core(false);
                spa_hook_remove(&listener);
                pw_proxy_destroy(proxy);

                return ((error < 0) || (link_res < 0)) ? STATUS_UNKNOWN_ERR : STATUS_OK;
            }

            status_t backend_t::disconnect_ports(audio::backend_t *self, const char *source, const char *destination)
            {
                if ((source == NULL) || (destination == NULL))
                    return STATUS_BAD_ARGUMENTS;

                backend_t * const back      = cast(self);
                pw_thread_loop_lock(back->pContextThreadLoop);
                lsp_finally { pw_thread_loop_unlock(back->pContextThreadLoop); };

                // Find source and destination port and their matches
                const pipewire::port_t * const src  = back->sRegistry.find_port(source, PORT_DIR_OUT);
                if (src == NULL)
                    return STATUS_NOT_FOUND;
                const pipewire::port_t * const dst  = back->sRegistry.find_port(destination, PORT_DIR_IN);
                if (dst == NULL)
                    return STATUS_NOT_FOUND;
                if ((src->nFlags & PORT_TYPE_MASK) != (dst->nFlags & PORT_TYPE_MASK))
                    return STATUS_BAD_TYPE;

                const link_t * link = back->sRegistry.find_link(src, dst);
                if (link == NULL)
                    return STATUS_NOT_FOUND;

                pw_registry_destroy(back->pRegistry, link->nID);
                return back->sync_core(false);
            }

            size_t backend_t::audio_buffers_count(audio::backend_t *self, port_id_t port_id)
            {
                backend_t * const back  = cast(self);
                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return 0;

                port_t * const port = &back->vPorts[port_id];
                if ((port->nType == PORT_TYPE_FREE) ||
                    ((port->nType & PORT_TYPE_MASK) != PORT_TYPE_AUDIO))
                    return 0;

                pw_buffer * const buf = port->pBuffer;
                return (buf != NULL) ? buf->buffer->n_datas : 0;
            }

            float *backend_t::get_audio_buffer(audio::backend_t *self, port_id_t port_id, size_t index)
            {
                backend_t * const back  = cast(self);
                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return NULL;

                port_t * const port = &back->vPorts[port_id];
                if ((port->nType == PORT_TYPE_FREE) ||
                    ((port->nType & PORT_TYPE_MASK) != PORT_TYPE_AUDIO))
                    return NULL;

                // Ensure that we have data to return
                pw_buffer * const buf = port->pBuffer;
                if ((buf == NULL) || (buf->buffer->n_datas < index))
                    return NULL;

                return static_cast<float *>(buf->buffer->datas[index].data);
            }

            status_t backend_t::read_midi_event(audio::backend_t *self, port_id_t port_id, midi_event_t *event, uint32_t *index)
            {
                if ((event == NULL) || (index == NULL))
                    return STATUS_BAD_ARGUMENTS;

                backend_t * const back  = cast(self);
                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return STATUS_NO_DATA;

                port_t * const port = &back->vPorts[port_id];
                if ((port->nType != PORT_MIDI_IN) && (port->nType != PORT_MIDI2_IN))
                    return STATUS_NO_DATA;

                // Ensure that we have data to return
                pw_buffer * const buf = port->pBuffer;
                if ((buf == NULL) || (buf->buffer->n_datas < 1))
                    return STATUS_NO_DATA;

                // Prepare pointers
                spa_data * const data       = &buf->buffer->datas[0];
                uint8_t *src                = static_cast<uint8_t *>(data->data);
                const uint8_t * end         = &src[data->chunk->size];

                // Check sequence type
                spa_pod_sequence * const seq= reinterpret_cast<spa_pod_sequence *>(src);
                if ((src > end) || (seq->pod.type != SPA_TYPE_Sequence) || (seq->pod.size <= sizeof(spa_pod_sequence_body)))
                    return STATUS_NO_DATA;
                end                         = lsp_min(end, &src[seq->pod.size + sizeof(spa_pod)]);
                src                        += sizeof(spa_pod_sequence);

                // Check that we are still in the valid range
                uint8_t *item               = &src[*index];
                if (item > end)
                    return STATUS_OVERFLOW;
                else if (item == end)
                    return STATUS_NO_DATA;

                // Fetch new record
                const uint32_t req_type     = (port->nType == PORT_MIDI_IN) ? SPA_CONTROL_Midi : SPA_CONTROL_UMP;
                while (true)
                {
                    // Get control header
                    control_header_t * const hdr    = advance_ptr<control_header_t>(item, 1);
                    spa_pod * const pod             = advance_ptr<spa_pod>(item, 1);
                    if (item > end)
                        return STATUS_NO_DATA;

                    // Check that POD contains the desired MIDI event
                    if (hdr->type == req_type)
                    {
                        event->timestamp                = hdr->timestamp;
                        event->size                     = pod->size;
                        event->data                     = item;

                        const uint32_t pod_size         = align_size(pod->size, SPA_POD_ALIGN);
                        *index                          = &item[pod_size] - src;
                        return STATUS_OK;
                    }
                    else
                        item                           += pod->size;
                }


                return STATUS_NO_DATA;
            }

            uint8_t *backend_t::write_midi_event(audio::backend_t *self, port_id_t port_id, uint32_t timestamp, uint32_t size)
            {
                backend_t * const back  = cast(self);
                if ((port_id < 0) || (port_id >= back->nPortCapacity))
                    return NULL;

                port_t * const port = &back->vPorts[port_id];
                if ((port->nType != PORT_MIDI_OUT) && (port->nType != PORT_MIDI2_OUT))
                    return NULL;

                // Ensure that we have data to return
                pw_buffer * const buf = port->pBuffer;
                if ((buf == NULL) || (buf->buffer->n_datas < 1))
                    return NULL;

                // Ensure that we have enough space to place the data
                spa_data * const data       = &buf->buffer->datas[0];
                const uint32_t padded_size  = align_size(size, SPA_POD_ALIGN);
                const uint32_t pod_size     =
                    sizeof(control_header_t) +
                    sizeof(spa_pod) +
                    padded_size;
                const uint32_t new_size     = data->chunk->size + pod_size;
                if ((new_size + sizeof(spa_pod_sequence)) > data->maxsize)
                {
                    lsp_warn("midi port id=%s buffer overflow: too many data", port->sID);
                    return NULL;
                }

                // Obtain destination buffer to store data
                uint8_t *dst            = static_cast<uint8_t *>(data->data);
                if (dst == NULL)
                    return NULL;
                dst                    += data->chunk->size + sizeof(spa_pod_sequence);
                data->chunk->size       = new_size;

                // Fill event header
                control_header_t * const hdr    = advance_ptr<control_header_t>(dst, 1);
                hdr->timestamp          = timestamp;
                hdr->type               = (port->nType == PORT_MIDI_OUT) ? SPA_CONTROL_Midi : SPA_CONTROL_UMP;

                // Fill pod data
                spa_pod * const pod     = advance_ptr<spa_pod>(dst, 1);
                pod->size               = size;
                pod->type               = SPA_TYPE_Bytes;

                // Pad data with zeros
                for (; size < padded_size; ++size)
                    dst[size]               = 0;

                return dst;
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

                // Check that we need to synchronize node name
                bool sync_node_name = false;
                if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0)
                {
                    if ((back->pFilter != NULL) && (id == pw_filter_get_node_id(back->pFilter)))
                        sync_node_name  = back->sRegistry.find_node_by_name(back->sClientName) != NULL;
                }
                else if (strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0)
                {
                    const char *metadata_name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
                    if (metadata_name == NULL)
                        return;

                    if ((strcmp(metadata_name, "default") == 0) && (back->pMetadata == NULL))
                    {
                        back->pMetadata = static_cast<pw_metadata *>(
                            pw_registry_bind(
                                back->pRegistry,
                                id, type, PW_VERSION_METADATA, 0));

                        if (back->pMetadata == NULL)
                            return;

                        pw_proxy_add_listener(
                            to_pw_proxy(back->pMetadata),
                            &back->vHooks[HOOK_METADATA_PROXY],
                            &metadata_proxy_events, back);
                        pw_metadata_add_listener(
                            back->pMetadata,
                            &back->vHooks[HOOK_METADATA],
                            &metadata_events, back);
                    }
                }

                const status_t res = back->sRegistry.process_add(id, permissions, type, version, props);
                if (res == STATUS_OK)
                {
                    const node_t * const node = (sync_node_name) ?
                        back->sRegistry.find_node_by_id(pw_filter_get_node_id(back->pFilter)) : NULL;
                    if (node != NULL)
                    {
                        // Update client name
                        char * new_name = strdup(node->sUID);
                        if (new_name != NULL)
                        {
                            lsp_trace("Node name updated from '%s' to '%s'", back->sClientName, new_name);
                            free(back->sClientName);
                            back->sClientName = new_name;
                        }

                        // Update global port identifiers
                        for (port_id_t i=0; i<back->nPortCapacity; ++i)
                        {
                            port_t * const port = &back->vPorts[i];
                            if (port->nType == PORT_TYPE_FREE)
                                continue;

                            new_name        = back->make_port_full_id(port->sID);
                            if (new_name != NULL)
                            {
                                if (port->sFullId != NULL)
                                    free(port->sFullId);
                                port->sFullId   = new_name;
                            }
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

            void backend_t::on_metadata_destroy(void *data)
            {
                backend_t * const back  = cast(data);
                pw_proxy_destroy(to_pw_proxy(back->pMetadata));
                back->pMetadata         = NULL;
            }

            void backend_t::on_metadata_removed(void *data)
            {
                backend_t * const back  = cast(data);
                if (back->pMetadata != NULL)
                {
                    spa_hook_remove(&back->vHooks[HOOK_METADATA_PROXY]);
                    spa_hook_remove(&back->vHooks[HOOK_METADATA]);
                }
            }

            int backend_t::on_metadata_property(void *self, uint32_t subject, const char *key, const char *type, const char *value)
            {
                backend_t * const back  = cast(self);
                back->sRegistry.process_metadata(subject, key, type, value);
                return 0;
            }

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

            void backend_t::update_sample_rate(const struct spa_pod *param) noexcept
            {
                spa_audio_info_raw info;
                if (spa_format_audio_raw_parse(param, &info) < 0)
                    return;

                lsp_trace("Changed sample rate to: %d\n", int(info.rate));
                if (sIOParams.sample_rate == info.rate)
                    return;

                // Update sample rate value
                sIOParams.sample_rate   = info.rate;
                if ((pCallbacks == NULL) || (pCallbacks->on_io_changed == NULL))
                    return;

                // Stop data loop first and deactivate filter
                int error = pw_filter_set_active(pFilter, false);
                if (error >= 0)
                    error = pw_data_loop_stop(pAudioDataLoop);
                if (error < 0)
                {
                    notify_connection_lost(false);
                    return;
                }

                // Issue IO changed callback
                status_t res = pCallbacks->on_io_changed(pUserData, &sIOParams);
                if (res != STATUS_OK)
                {
                    notify_connection_lost(false);
                    return;
                }

                // Activate filter and data loop back
                error = pw_filter_set_active(pFilter, true);
                if (error >= 0)
                    error = pw_data_loop_start(pAudioDataLoop);
                if (error < 0)
                {
                    lsp_error("Failed to update filter sample rate, error=%d", -error);
                    notify_connection_lost(false);
                }
            }

            void backend_t::update_latency(uint32_t latency) noexcept
            {
                // Stop audio data loop and deactivate filter
                int error = pw_filter_set_active(pFilter, false);
                if (error >= 0)
                    error = pw_data_loop_stop(pAudioDataLoop);

                // Update filter parameters
                if (error >= 0)
                {
                    pod_builder builder;
                    const spa_pod *pod = builder.make_process_latency_pod(latency, sIOParams.sample_rate);
                    error = pw_filter_update_params(pFilter, NULL, &pod, 1);
                    if (error >= 0)
                    {
                        lsp_trace("Updated filter latency %d -> %d", int(nLatency), int(latency));
                        nLatency    = latency;
                    }
                }

                // Activate filter and data loop back
                if (error >= 0)
                    error = pw_filter_set_active(pFilter, true);
                if (error >= 0)
                    error = pw_data_loop_start(pAudioDataLoop);

                // Report error if occurred
                if (error < 0)
                {
                    lsp_error("Failed to update filter latency, error=%d", -error);
                    notify_connection_lost(false);
                }
            }

            void backend_t::on_filter_param_changed(void *self, void *port_data, uint32_t id, const struct spa_pod *param)
            {
                lsp_trace("self=%p, port_data=%p, id=%d (%s), param=%p",
                    self, port_data, int(id), decode_spa_param_id(id), param);
                backend_t * const back = cast(self);

                const port_data_t * const handle = static_cast<port_data_t *>(port_data);
                if (handle != NULL)
                {
                    port_t * const port = (handle->nPortId < back->nPortCapacity) ?
                        &back->vPorts[handle->nPortId] : NULL;
                    if ((port != NULL) && (port->nType != PORT_TYPE_FREE))
                    {
                        lsp_trace("port nid=%d, id=%s, full_id=%s",
                            int(handle->nPortId), port->sID, port->sFullId);
                    }
                }
                else if (param != NULL)
                {
                    // Update filter params
                    if (id == SPA_PARAM_Format)
                        back->update_sample_rate(param);
                }
            }

            void backend_t::on_filter_add_buffer(void *self, void *port_data, struct pw_buffer *buffer)
            {
                lsp_trace("self=%p, port_data=%p, buffer=%p", self, port_data, buffer);
                lsp_trace("debug");
            }

            void backend_t::on_filter_remove_buffer(void *self, void *port_data, struct pw_buffer *buffer)
            {
                lsp_trace("self=%p, port_data=%p, buffer=%p", self, port_data, buffer);
                lsp_trace("debug");
            }

            void backend_t::on_filter_process(void *self, struct spa_io_position *position)
            {
                backend_t * const back  = cast(self);
                if ((back->pCallbacks == NULL) || (back->pCallbacks->on_process == NULL))
                    return;

                // Dequeue all port buffers
                const uint32_t samples  = position->clock.duration;
                for (port_id_t i=0; i<back->nPortCapacity; ++i)
                {
                    port_t * const port     = &back->vPorts[i];
                    if (port->nType == PORT_TYPE_FREE)
                        continue;

                    pw_buffer * const buf   = pw_filter_dequeue_buffer(port->pHandle);
                    if (buf == NULL)
                        continue;

                    port->pBuffer           = buf;
                    if (buf->buffer->n_datas < 1)
                        continue;

                    // Initialize output chunks if available
                    spa_data * const data   = &buf->buffer->datas[0];
                    if (port->nType == PORT_AUDIO_OUT)
                    {
                        data->chunk->offset     = 0;
                        data->chunk->size       = samples * sizeof(float);
                        data->chunk->stride     = sizeof(float);
                        data->chunk->flags      = 0;
                    }
                    else if ((port->nType == PORT_MIDI_OUT) || (port->nType == PORT_MIDI2_OUT))
                    {
                        data->chunk->offset     = 0;
                        data->chunk->size       = 0;
                        data->chunk->stride     = 1;
                        data->chunk->flags      = 0;
                    }
                }

                // TODO: update sIOPosition

                // Issue callback
                back->pCallbacks->on_process(back->pUserData, &back->sIOPosition, samples);

                // Queue all available port buffers
                for (port_id_t i=0; i<back->nPortCapacity; ++i)
                {
                    port_t * const port = &back->vPorts[i];
                    if ((port->nType == PORT_TYPE_FREE) || (port->pBuffer == NULL))
                        continue;

                    // Fill sequence header if MIDI is used
                    if ((port->nType == PORT_MIDI_OUT) || (port->nType == PORT_MIDI2_OUT))
                    {
                        // Obtain data
                        spa_data * const data           = &port->pBuffer->buffer->datas[0];
                        spa_pod_sequence * const seq    = static_cast<spa_pod_sequence *>(data->data);

                        // Fill sequence header and update chunk size
                        seq->pod.size                   = sizeof(spa_pod_sequence_body) + data->chunk->size;
                        seq->pod.type                   = SPA_TYPE_Sequence;
                        seq->body.unit                  = 0;
                        seq->body.pad                   = 0;
                        data->chunk->size              += sizeof(spa_pod_sequence);
                    }

                    pw_filter_queue_buffer(port->pHandle, port->pBuffer);
                    port->pBuffer       = NULL;
                }
            }

            void backend_t::on_filter_drained(void *self)
            {
                lsp_trace("self=%p", self);
                lsp_trace("debug");
            }

            void backend_t::on_filter_command(void *self, const struct spa_command *command)
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
                lsp_trace("debug");
            }

            void backend_t::on_notify_event(void *self, uint64_t count)
            {
                lsp_trace("self = %p", self);
            }

            void backend_t::on_link_error(void *data, int seq, int res, const char *message)
            {
                int *link_res = static_cast<int *>(data);
                *link_res = res;
            }

            void backend_t::notify_connection_lost(bool stop) noexcept
            {
                if (stop)
                {
                    pw_thread_loop_lock(pContextThreadLoop);
                    lsp_finally { pw_thread_loop_unlock(pContextThreadLoop); };
                    pw_data_loop_stop(pAudioDataLoop);
                }

                if ((pCallbacks != NULL) && (pCallbacks->on_connection_lost))
                    pCallbacks->on_connection_lost(pUserData);
            }

            void backend_t::on_ringbuffer_data_received(void *self, uint64_t count)
            {
                backend_t * const back  = cast(self);
                uint32_t latency        = back->nLatency;

                // Process all incoming messages
                message_t message;
                while (true)
                {
                    // Try to read next message
                    const status_t res  = read_message(&message, &back->sRingBuffer);
                    if (res == STATUS_NO_DATA)
                        break;

                    // Break on error
                    if (res != STATUS_OK)
                    {
                        lsp_error("Failed to read message from ring buffer, connection corrupted, code=%d", int(res));
                        back->notify_connection_lost(true);
                        return;
                    }

                    switch (message.header.type)
                    {
                        case MSG_LATENCY:
                            latency         = message.latency.latency;
                            break;
                        default:
                            lsp_error("Received unknown message from ring buffer, type=%d", int(message.header.type));
                            back->notify_connection_lost(true);
                            return;
                    }
                }

                // Synchronize state
                if (back->nLatency != latency)
                    back->update_latency(latency);
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

            backend_t::port_t *backend_t::find_port(const char *id) noexcept
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

            void backend_t::init_port(port_t *port) noexcept
            {
                port->nType             = PORT_TYPE_FREE;
                port->pHandle           = NULL;
                port->pBuffer           = NULL;
                port->sFullId           = NULL;
                port->sID[0]            = '\0';
            }

            backend_t::port_t *backend_t::alloc_port(const char *id, uint32_t flags) noexcept
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

            void backend_t::free_port(port_t *port) noexcept
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




