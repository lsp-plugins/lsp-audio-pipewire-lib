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

#include <lsp-plug.in/audio/pipewire/registry.h>

#include <lsp-plug.in/audio/iface/types.h>
#include <lsp-plug.in/audio/pipewire/impl/cast.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/stdlib/string.h>

#include <pipewire/client.h>
#include <pipewire/keys.h>
#include <pipewire/link.h>
#include <pipewire/node.h>
#include <pipewire/port.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            template <typename T>
            static inline void cfree(const T * ptr)
            {
                if (ptr != NULL)
                    free(const_cast<T *>(ptr));
            }

            static uint32_t fetch_pw_id(const spa_dict * dict, const char *key, uint32_t dfl = SPA_ID_INVALID)
            {
                const char *value = spa_dict_lookup(dict, key);
                if ((value == NULL) || (value[0] == '\0'))
                    return dfl;

                uint32_t res = 0;
                while (*value != '\0')
                {
                    const char c   = *(value++);
                    if ((c < '0') || (c > '9'))
                        return dfl;
                    const uint32_t nres = (res * 10) + (c - '0');
                    if (nres < res)
                        return dfl;
                    res                 = nres;
                }
                return res;
            }

            static uint32_t fetch_port_flags(const spa_dict * dict, uint32_t dfl = PORT_TYPE_UNKNOWN)
            {
                uint32_t flags      = 0;

                // Parse direction
                const char *dir     = spa_dict_lookup(dict, PW_KEY_PORT_DIRECTION);
                if (dir == NULL)
                    return dfl;
                else if (strcmp(dir, "in") == 0)
                    flags              |= PORT_DIR_IN;
                else if (strcmp(dir, "out") == 0)
                    flags              |= PORT_DIR_OUT;
                else
                    return dfl;

                // Parse format
                const char *fmt     = spa_dict_lookup(dict, PW_KEY_FORMAT_DSP);
                if (fmt == NULL)
                    return dfl;
                else if (strcmp(fmt, PORT_FORMAT_DSP_AUDIO) == 0)
                    flags              |= PORT_TYPE_AUDIO;
                else if (strcmp(fmt, PORT_FORMAT_DSP_MIDI) == 0)
                    flags              |= PORT_TYPE_MIDI;
                else
                    return dfl;

                return flags;
            }

            registry::registry() noexcept
            {
                construct();
            }

            registry::~registry() noexcept
            {
                destroy();
            }

            void registry::construct() noexcept
            {
                init_storage(vClients);
                init_storage(vNodes);
                init_storage(vPorts);
                init_storage(vLinks);
            }

            void registry::destroy() noexcept
            {
                destroy_storage<client_t>(vClients);
                destroy_storage<node_t>(vNodes);
                destroy_storage<port_t>(vPorts);
                destroy_storage<link_t>(vLinks);
            }

            inline void registry::init_storage(storage_t & storage) noexcept
            {
                storage.vObjects        = NULL;
                storage.nCount          = 0;
                storage.nCapacity       = 0;
            }

            template <typename T>
            inline void registry::destroy_storage(storage_t & storage) noexcept
            {
                for (size_t i=0; i<storage.nCount; ++i)
                    destroy(static_cast<T *>(storage.vObjects[i]));
                cfree(storage.vObjects);
                storage.nCount      = 0;
                storage.nCapacity   = 0;
            }

            template <typename T>
            inline T *registry::find_by_id(storage_t & storage, uint32_t id) noexcept
            {
                object_t * const obj = find_object_by_id(storage, id);
                return (obj != NULL) ? static_cast<T *>(obj) : NULL;
            }

            object_t *registry::find_object_by_id(storage_t & storage, uint32_t id) noexcept
            {
                ssize_t first=0, last = ssize_t(storage.nCount) - 1;
                while (first <= last)
                {
                    const int32_t middle = (first + last) >> 1;
                    object_t * const obj = storage.vObjects[middle];
                    if (obj->nID < id)
                        last                = middle - 1;
                    else if (obj->nID > id)
                        first               = middle + 1;
                    else
                        return obj;
                }
                return NULL;
            }

            uint32_t registry::storage_index_of(storage_t & storage, uint32_t id) noexcept
            {
                ssize_t first=0, last = ssize_t(storage.nCount) - 1;
                while (first <= last)
                {
                    const int32_t middle = (first + last) >> 1;
                    object_t * const obj = storage.vObjects[middle];
                    if (obj->nID < id)
                        last                = middle - 1;
                    else if (obj->nID > id)
                        first               = middle + 1;
                    else
                        return uint32_t(middle);
                }

                return uint32_t(first);
            }

            bool registry::store_object(storage_t & storage, object_t * & old, object_t *object) noexcept
            {
                const uint32_t items = storage.nCount;
                uint32_t index       = storage_index_of(storage, object->nID);

                // Replace value if matches
                if (index < items)
                {
                    const uint32_t dst_index = storage.vObjects[index]->nID;
                    if (index == dst_index)
                    {
                        old = storage.vObjects[index];
                        storage.vObjects[index] = object;
                        return true;
                    }
                    else if (index > dst_index)
                        ++index;
                }

                // Ensure that we have enough capacity to insert
                if (storage.nCount >= storage.nCapacity)
                {
                    const uint32_t new_cap = lsp_max(storage.nCapacity << 1, uint32_t(4));
                    object_t **new_items    = realloc_count<object_t *>(storage.vObjects, new_cap);
                    if (new_items == NULL)
                        return false;

                    storage.vObjects        = new_items;
                    storage.nCapacity       = new_cap;
                }

                // Insert element
                if (index < storage.nCount)
                    memmove(
                        &storage.vObjects[index+1],
                        &storage.vObjects[index],
                        (storage.nCount - index) * sizeof(object_t *));

                storage.vObjects[index++]  = object;
                ++storage.nCount;

                return true;
            }

            template <typename T>
            bool registry::add_to_storage(storage_t & storage, T *object) noexcept
            {
                object_t *old = NULL;
                const bool result = store_object(storage, old, object);
                if (old != NULL)
                    destroy(static_cast<T *>(old));
                return result;
            }

            template <typename T>
            inline T *registry::remove_by_id(storage_t & storage, uint32_t id) noexcept
            {
                const uint32_t index    = storage_index_of(storage, id);
                if (index >= storage.nCapacity)
                    return NULL;
                object_t * const obj    = storage.vObjects[index];
                if (obj->nID != id)
                    return NULL;

                --storage.nCount;
                memmove(
                    &storage.vObjects[index],
                    &storage.vObjects[index + 1],
                    (storage.nCount - index) * sizeof(object_t *));

                return static_cast<T *>(obj);
            }

            inline void registry::destroy(client_t *item) noexcept
            {
                if (item == NULL)
                    return;
                cfree(item->sName);
                cfree(item->sUID);
                cfree(item);
            }

            inline void registry::destroy(node_t *item) noexcept
            {
                if (item == NULL)
                    return;
                cfree(item->sName);
                cfree(item->sDesc);
                cfree(item->sNick);
                cfree(item->sUID);
                cfree(item);
            }

            inline void registry::destroy(port_t *item) noexcept
            {
                if (item == NULL)
                    return;
                cfree(item->sName);
                cfree(item);
            }

            inline void registry::destroy(link_t *item) noexcept
            {
                if (item == NULL)
                    return;
                cfree(item);
            }

            inline client_t *registry::alloc_client() noexcept
            {
                client_t * const item   = malloc_count<client_t>(1);
                if (item != NULL)
                {
                    item->nID               = SPA_ID_INVALID;
                    item->sName             = NULL;
                    item->sUID              = NULL;
                }
                return item;
            }

            inline node_t *registry::alloc_node() noexcept
            {
                node_t * const item     = malloc_count<node_t>(1);
                if (item != NULL)
                {
                    item->nID               = SPA_ID_INVALID;
                    item->nClientID         = SPA_ID_INVALID;
                    item->sName             = NULL;
                    item->sDesc             = NULL;
                    item->sNick             = NULL;
                    item->sUID              = NULL;
                }
                return item;
            }

            inline port_t   *registry::alloc_port() noexcept
            {
                port_t * const item     = malloc_count<port_t>(1);
                if (item != NULL)
                {
                    item->nID               = SPA_ID_INVALID;
                    item->nNodeID           = SPA_ID_INVALID;
                    item->nPortID           = 0;
                    item->nLinks            = 0;
                    item->nFlags            = 0;
                    item->sName             = NULL;
                }
                return item;
            }

            inline link_t   *registry::alloc_link() noexcept
            {
                link_t * const item     = malloc_count<link_t>(1);
                if (item != NULL)
                {
                    item->nID               = SPA_ID_INVALID;
                    item->nInNodeID         = SPA_ID_INVALID;
                    item->nInPortID         = SPA_ID_INVALID;
                    item->nOutNodeID        = SPA_ID_INVALID;
                    item->nOutPortID        = SPA_ID_INVALID;
                }
                return item;
            }

            status_t registry::process_add(uint32_t id, uint32_t permissions, const char *type, uint32_t version, const spa_dict *props)
            {
                if (id == SPA_ID_INVALID)
                    return STATUS_BAD_ARGUMENTS;

                if (strcmp(type, PW_TYPE_INTERFACE_Client) == 0)
                {
                    // Fetch properties
                    const char *client_name = spa_dict_lookup(props, PW_KEY_APP_NAME);
                    if (client_name == NULL)
                        return STATUS_BAD_ARGUMENTS;

                    // Allocate client
                    client_t *client        = alloc_client();
                    if (client == NULL)
                        return STATUS_NO_MEM;
                    lsp_finally { destroy(client); };

                    // Initialize
                    client->nID     = id;
                    if ((client->sName = strdup(client_name)) == NULL)
                        return STATUS_NO_MEM;
                    if ((client->sUID = strfmt("%s-%u", client_name, (unsigned int)(id))) == NULL)
                        return STATUS_NO_MEM;

                    // Add to storage
                    if (!add_to_storage(vClients, client))
                        return STATUS_NO_MEM;

                    // Do not free client
                    lsp_trace(
                        "Registered client id=%d, name=%s, uid=%s",
                        int(client->nID), client->sName, client->sUID);
                    client  = NULL;
                }
                else if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0)
                {
                    // Fetch properties
                    const char *node_name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
                    if (node_name == NULL)
                        return STATUS_BAD_ARGUMENTS;

                    const uint32_t client_id = fetch_pw_id(props, PW_KEY_CLIENT_ID);
                    const char *node_desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
                    const char *node_nick = spa_dict_lookup(props, PW_KEY_NODE_NICK);
                    const char *node_app_name = spa_dict_lookup(props, PW_KEY_APP_NAME);
                    if (node_app_name != NULL)
                    {
                        if (node_nick == NULL)
                            node_nick       = node_app_name;
                        else if (node_desc == NULL)
                            node_desc       = node_app_name;
                    }
                    const char *uid_seed    = node_nick;
                    if (uid_seed == NULL)
                    {
                        uid_seed                = node_desc;
                        if (uid_seed == NULL)
                        {
                            uid_seed                = node_app_name;
                            if (uid_seed == NULL)
                                uid_seed                = node_name;
                        }
                    }

                    // Allocate node
                    node_t *node            = alloc_node();
                    if (node == NULL)
                        return STATUS_NO_MEM;
                    lsp_finally { destroy(node); };

                    // Initialize
                    node->nID               = id;
                    node->nClientID         = client_id;

                    if ((node->sName = strdup(node_name)) == NULL)
                        return STATUS_NO_MEM;
                    if (node_desc != NULL)
                    {
                        if ((node->sDesc = strdup(node_desc)) == NULL)
                            return STATUS_NO_MEM;
                    }
                    if (node_nick != NULL)
                    {
                        if ((node->sNick = strdup(node_nick)) == NULL)
                            return STATUS_NO_MEM;
                    }
                    if ((node->sUID = strfmt("%s-%u", uid_seed, (unsigned int)(id))) == NULL)
                        return STATUS_NO_MEM;

                    // Add to storage
                    if (!add_to_storage(vNodes, node))
                        return STATUS_NO_MEM;

                    // Do not free node
                    lsp_trace(
                        "Registered node id=%d, name=%s, uid=%s",
                        int(node->nID), node->sName, node->sUID);
                    node  = NULL;
                }
                else if (strcmp(type, PW_TYPE_INTERFACE_Port) == 0)
                {
                    // Fetch properties
                    const char *port_name = spa_dict_lookup(props, PW_KEY_PORT_NAME);
                    if (port_name == NULL)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t node_id = fetch_pw_id(props, PW_KEY_NODE_ID);
                    if (node_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t port_id = fetch_pw_id(props, PW_KEY_PORT_ID);
                    if (port_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t flags = fetch_port_flags(props);
                    if (flags == uint32_t(PORT_TYPE_UNKNOWN))
                        return STATUS_UNSUPPORTED_FORMAT;

                    // Allocate port
                    port_t *port            = alloc_port();
                    if (port == NULL)
                        return STATUS_NO_MEM;
                    lsp_finally { destroy(port); };

                    // Initialize
                    port->nID               = id;
                    port->nNodeID           = node_id;
                    port->nPortID           = port_id;
                    port->nFlags            = flags;
                    if ((port->sName = strdup(port_name)) == NULL)
                        return STATUS_NO_MEM;

                    // Add to storage
                    if (!add_to_storage(vPorts, port))
                        return STATUS_NO_MEM;

                    // Do not free port
                #ifdef LSP_TRACE
                    node_t * const node     = find_by_id<node_t>(vNodes, port->nNodeID);
                    lsp_trace("Registered port id=%d, name=%s, node id=%d, name=%s, uid=%s",
                        int(port->nID), port->sName,
                        int(port->nNodeID),
                        (node != NULL) ? node->sName : "<null>",
                        (node != NULL) ? node->sUID: "<null>");
                #endif /* LSP_TRACE */
                    port  = NULL;
                }
                else if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0)
                {
                    // Fetch properties
                    const uint32_t in_node_id = fetch_pw_id(props, PW_KEY_LINK_INPUT_NODE);
                    if (in_node_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t in_port_id = fetch_pw_id(props, PW_KEY_LINK_INPUT_PORT);
                    if (in_port_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t out_node_id = fetch_pw_id(props, PW_KEY_LINK_OUTPUT_NODE);
                    if (out_node_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;
                    const uint32_t out_port_id = fetch_pw_id(props, PW_KEY_LINK_OUTPUT_PORT);
                    if (out_port_id == SPA_ID_INVALID)
                        return STATUS_BAD_ARGUMENTS;

                    // Allocate link
                    link_t *link            = alloc_link();
                    if (link == NULL)
                        return STATUS_NO_MEM;
                    lsp_finally { destroy(link); };

                    // Initialize
                    link->nID               = id;
                    link->nInNodeID         = in_node_id;
                    link->nInPortID         = in_port_id;
                    link->nOutNodeID        = out_node_id;
                    link->nOutPortID        = out_port_id;

                    // Add to storage
                    if (!add_to_storage(vLinks, link))
                        return STATUS_NO_MEM;

                    // Do not free link
                #ifdef LSP_TRACE
                    node_t * const in_node      = find_by_id<node_t>(vNodes, link->nInNodeID);
                    node_t * const out_node     = find_by_id<node_t>(vNodes, link->nOutNodeID);
                    port_t * const in_port      = find_by_id<port_t>(vPorts, link->nInPortID);
                    port_t * const out_port     = find_by_id<port_t>(vPorts, link->nOutPortID);
                    lsp_trace("Registered link id=%d, %s (id=%d):%s (id=%d) -> %s (id=%d):%s (id=%d)",
                        int(link->nID),
                        (in_node != NULL) ? in_node->sName : "<null>", int(link->nInNodeID),
                        (in_port != NULL) ? in_port->sName : "<null>", int(link->nInPortID),
                        (out_node != NULL) ? out_node->sName : "<null>", int(link->nOutNodeID),
                        (out_port != NULL) ? out_port->sName : "<null>", int(link->nOutPortID));
                #endif /* LSP_TRACE */
                    link = NULL;
                }

                return STATUS_OK;
            }

            status_t registry::process_remove(uint32_t id)
            {
                client_t * const client     = remove_by_id<client_t>(vClients, id);
                if (client != NULL)
                {
                    lsp_trace("Removed client id=%d, name=%s, uid=%s", int(client->nID), client->sName, client->sUID);
                    destroy(client);
                    return STATUS_OK;
                }

                node_t * const node         = remove_by_id<node_t>(vNodes, id);
                if (node != NULL)
                {
                    lsp_trace("Removed node id=%d, name=%s, uid=%s", int(node->nID), node->sName, node->sUID);
                    destroy(node);
                    return STATUS_OK;
                }

                port_t * const port         = remove_by_id<port_t>(vPorts, id);
                if (port != NULL)
                {
                #ifdef LSP_TRACE
                    node_t * const node     = find_by_id<node_t>(vNodes, port->nNodeID);
                    lsp_trace("Removed port id=%d, name=%s, node id=%d, name=%s, uid=%s",
                        int(port->nID), port->sName,
                        int(port->nNodeID),
                        (node != NULL) ? node->sName : "<null>",
                        (node != NULL) ? node->sUID: "<null>");
                #endif /* LSP_TRACE */
                    destroy(port);
                    return STATUS_OK;
                }

                link_t * const link         = remove_by_id<link_t>(vLinks, id);
                if (link != NULL)
                {
                #ifdef LSP_TRACE
                    node_t * const in_node      = find_by_id<node_t>(vNodes, link->nInNodeID);
                    node_t * const out_node     = find_by_id<node_t>(vNodes, link->nOutNodeID);
                    port_t * const in_port      = find_by_id<port_t>(vPorts, link->nInPortID);
                    port_t * const out_port     = find_by_id<port_t>(vPorts, link->nOutPortID);
                    lsp_trace("Removed link id=%d, %s (id=%d):%s (id=%d) -> %s (id=%d):%s (id=%d)",
                        int(link->nID),
                        (in_node != NULL) ? in_node->sName : "<null>", int(link->nInNodeID),
                        (in_port != NULL) ? in_port->sName : "<null>", int(link->nInPortID),
                        (out_node != NULL) ? out_node->sName : "<null>", int(link->nOutNodeID),
                        (out_port != NULL) ? out_port->sName : "<null>", int(link->nOutPortID));
                #endif /* LSP_TRACE */
                    destroy(link);
                    return STATUS_OK;
                }

                return STATUS_NOT_FOUND;
            }

            const node_t *registry::find_node_by_id(uint32_t id) const
            {
                registry * const self = const_cast<registry *>(this);
                return find_by_id<node_t>(self->vNodes, id);
            }

            const node_t *registry::find_node_by_name(const char *name) const
            {
                for (size_t i=0, n=vNodes.nCount; i<n; ++i)
                {
                    const node_t * const node = static_cast<const node_t *>(vNodes.vObjects[i]);
                    if (strcmp(node->sName, name) == 0)
                        return node;
                }
                return NULL;
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


