/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 1 мая 2026 г.
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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_CAST_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_CAST_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/iface/types.h>
#include <lsp-plug.in/audio/pipewire/backend.h>
#include <pw-headers/pipewire/proxy.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            constexpr const char *prop_true                     = "true";
            constexpr const char *prop_false                    = "false";

            constexpr const char *BACKEND_MEDIA_TYPE            = "Audio";
            constexpr const char *BACKEND_MEDIA_CATEGORY        = "Duplex";
            constexpr const char *BACKEND_MEDIA_ROLE            = "DSP";
            constexpr const char *BACKEND_CLIENT_API            = "native";

            constexpr const char *BACKEND_CONFIG_FILE           = "client.conf";

            constexpr const char *PORT_FORMAT_DSP_AUDIO         = "32 bit float mono audio";
            constexpr const char *PORT_FORMAT_DSP_MIDI          = "8 bit raw midi";
            constexpr const char *PORT_FORMAT_DSP_MIDI2         = "32 bit raw UMP";
            constexpr const char *PORT_FORMAT_DSP_OTHER         = "other";

            constexpr const char *DEFAULT_DEVICE_NAME           = "system";
            constexpr const char *DEFAULT_SERVER_NAME           = "default";

            constexpr const char *METADATA_DEFAULT_NAME         = "default";

            constexpr const char *ACTION_UPDATE_PROPERTIES      = "update-props";

            constexpr const char *BACKEN_DEFAULT_NODE_GROUP     = "group.dsp.0";

            constexpr const uint32_t RING_BUFFER_SIZE           = 0x1000;

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

            static inline const char *port_format_dsp(size_t flags)
            {
                switch (flags & PORT_TYPE_MASK)
                {
                    case PORT_TYPE_AUDIO:
                        return PORT_FORMAT_DSP_AUDIO;
                    case PORT_TYPE_MIDI:
                        return PORT_FORMAT_DSP_MIDI;
                    case PORT_TYPE_MIDI2:
                        return PORT_FORMAT_DSP_MIDI2;
                    default:
                        break;
                }
                return PORT_FORMAT_DSP_OTHER;
            }

            inline const char * decode_spa_command_type(uint32_t type)
            {
                switch (type)
                {
                    case SPA_TYPE_COMMAND_Device:
                        return "SPA_TYPE_COMMAND_Device";
                    case SPA_TYPE_COMMAND_Node:
                        return "SPA_TYPE_COMMAND_Node";
                    default:
                        break;
                }
                return "unknown";
            }

            inline const char * decode_spa_node_command(uint32_t id)
            {
                #define V(key) \
                    case key: return # key;

                switch (id)
                {
                    V(SPA_NODE_COMMAND_Suspend)
                    V(SPA_NODE_COMMAND_Pause)
                    V(SPA_NODE_COMMAND_Start)
                    V(SPA_NODE_COMMAND_Enable)
                    V(SPA_NODE_COMMAND_Disable)
                    V(SPA_NODE_COMMAND_Flush)
                    V(SPA_NODE_COMMAND_Drain)
                    V(SPA_NODE_COMMAND_Marker)
                    V(SPA_NODE_COMMAND_ParamBegin)
                    V(SPA_NODE_COMMAND_ParamEnd)
                    V(SPA_NODE_COMMAND_RequestProcess)
                    default: break;
                }
                #undef V

                return "unknown";
            }

            inline const char * decode_spa_param_id(uint32_t id)
            {
                #define V(key) \
                    case key: return # key;

                switch (id)
                {
                    V(SPA_PARAM_Invalid)
                    V(SPA_PARAM_PropInfo)
                    V(SPA_PARAM_Props)
                    V(SPA_PARAM_EnumFormat)
                    V(SPA_PARAM_Format)
                    V(SPA_PARAM_Buffers)
                    V(SPA_PARAM_Meta)
                    V(SPA_PARAM_IO)
                    V(SPA_PARAM_Profile)
                    V(SPA_PARAM_EnumPortConfig)
                    V(SPA_PARAM_PortConfig)
                    V(SPA_PARAM_EnumRoute)
                    V(SPA_PARAM_Route)
                    V(SPA_PARAM_Control)
                    V(SPA_PARAM_Latency)
                    V(SPA_PARAM_ProcessLatency)
                    V(SPA_PARAM_Tag)
                    default: break;
                }
                #undef V

                return "unknown";
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_CAST_H_ */
