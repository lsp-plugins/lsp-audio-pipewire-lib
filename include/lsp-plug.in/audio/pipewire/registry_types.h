/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 3 мая 2026 г.
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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_TYPES_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_TYPES_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/common/types.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            typedef struct object_t     object_t;
            typedef struct client_t     client_t;
            typedef struct node_t       node_t;
            typedef struct port_t       port_t;
            typedef struct link_t       link_t;

            struct object_t
            {
                uint32_t                nID;        // ID of object
            };

            // Device
            struct device_t: public object_t
            {
                uint32_t                nClientID;  // ID of a client
                uint32_t                nFactoryID; // ID of a factory
                const char             *sName;      // Name of a related node
                const char             *sNick;      // Nickname of a related node
                const char             *sDesc;      // Description of a related node
                const char             *sAPI;       // API
                const char             *sMediaClass;// Media class
                const char             *sMediaRole; // Media role
            };

            // Client
            struct client_t: public object_t
            {
                const char             *sName;      // Name of a client
                const char             *sUID;       // Unique string identifier
            };

            // Node
            struct node_t: public object_t
            {
                uint32_t                nClientID;  // ID of a client
                const char             *sName;      // Node name
                const char             *sDesc;      // Node description
                const char             *sNick;      // Node nickname
                const char             *sUID;       // Unique string identifier
            };

            // Port
            struct port_t: public object_t
            {
                uint32_t                nNodeID;    // ID of a node
                uint32_t                nPortID;    // ID of a port
                uint32_t                nLinks;     // Number of links
                uint32_t                nFlags;     // Port flags (type, direction)
                const char             *sName;      // The name of a port
                const char             *sSystemId;  // System name of a port
            };

            // Link
            struct link_t: public object_t
            {
                uint32_t                nInNodeID;  // Source node identifier
                uint32_t                nInPortID;  // Source port identifier
                uint32_t                nOutNodeID; // Destination node identifier
                uint32_t                nOutPortID; // Destination port identifier
            };

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */



#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_TYPES_H_ */
