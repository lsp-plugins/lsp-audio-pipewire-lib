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
#include <lsp-plug.in/audio/pipewire/impl/pw-defs.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/stdlib/string.h>

#include <pw-headers/pipewire/client.h>
#include <pw-headers/pipewire/keys.h>
#include <pw-headers/pipewire/link.h>
#include <pw-headers/pipewire/node.h>
#include <pw-headers/pipewire/port.h>
#include <pw-headers/spa/utils/json.h>

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

            static bool fetch_pw_bool(const spa_dict * dict, const char *key, bool dfl = false)
            {
                const char *value = spa_dict_lookup(dict, key);
                if ((value == NULL) || (value[0] == '\0'))
                    return dfl;

                return strcmp(value, prop_true) == 0;
            }

            static uint32_t fetch_port_flags(const spa_dict * dict, uint32_t dfl = PORT_TYPE_UNKNOWN)
            {
                uint32_t flags      = 0;

                // Parse direction
                const char *dir     = spa_dict_lookup(dict, PW_KEY_PORT_DIRECTION);
                if (dir == NULL)
                    return dfl;
                else if (strcmp(dir, PW_PORT_DIRECTION_IN) == 0)
                    flags              |= PORT_DIR_IN;
                else if (strcmp(dir, PW_PORT_DIRECTION_OUT) == 0)
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
                {
                    bool is_midi2   = false;
                    if (fetch_pw_bool(dict, PW_KEY_CONTROL_UMP, false))
                        is_midi2        = true;

                    flags              |= (is_midi2) ? PORT_TYPE_MIDI2 : PORT_TYPE_MIDI;
                }
                else
                    return dfl;

                return flags;
            }

            static int find_spa_json_object(spa_json *iter, const char *key, const char **value)
            {
                spa_json obj = SPA_JSON_SAVE(iter);
                int res, len = strlen(key) + 3;
                char * k = static_cast<char *>(alloca(len));
                if (!k)
                    return -ENOMEM;

                while ((res = spa_json_object_next(&obj, k, len, value)) > 0)
                    if (spa_streq(k, key))
                        return res;
                return -ENOENT;
            }

            static char * fetch_spa_string(const char *value, const char *key)
            {
                if ((value == NULL) || (key == NULL))
                    return NULL;

                // Find SPA object
                struct spa_json iter;
                if (spa_json_begin_object(&iter, value, strlen(value)) <= 0)
                    return NULL;

                const char *v = NULL;
                const int length = find_spa_json_object(&iter, key, &v);
                if (length < 0)
                    return NULL;

                // Allocate initial buffer
                const int buf_size = length + 1;
                char * buf = malloc_count<char>(buf_size);
                if (buf == NULL)
                    return NULL;
                lsp_finally {
                    if (buf != NULL)
                        free(buf);
                };

                // Parse string into buffer
                const int res = spa_json_parse_stringn(v, length, buf, buf_size);
                return (res < 0) ? NULL : release_ptr(buf);
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
                sDefaultSource      = NULL;
                sDefaultSink        = NULL;
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
                cfree(sDefaultSource);
                cfree(sDefaultSink);
                sDefaultSource          = NULL;
                sDefaultSink            = NULL;
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
                storage.vObjects        = NULL;
                storage.nCount          = 0;
                storage.nCapacity       = 0;
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
                    if (id < obj->nID)
                        last                = middle - 1;
                    else if (id > obj->nID)
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
                    if (id < obj->nID)
                        last                = middle - 1;
                    else if (id > obj->nID)
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
                    const uint32_t new_cap  = lsp_max(storage.nCapacity << 1, uint32_t(4));
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
                if (index >= storage.nCount)
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
                cfree(item->sSystemId);
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
                    item->sSystemId         = NULL;
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

            char *registry::make_unique_id(const char *prefix, uint32_t id) noexcept
            {
                // First check that there is no node with such name as the prefix
                if (!find_node_by_uid(prefix))
                    return strdup(prefix);

                // Try to add node id to the name
                char *uid = strfmt("%s-%u", prefix, (unsigned int)(id));
                if (uid == NULL)
                    return NULL;
                if (!find_node_by_uid(uid))
                    return uid;

                // Use additional index if the node with such identifier already exists
                for (int i=0; ; ++i)
                {
                    free(uid);
                    uid = strfmt("%s-%u-%d", prefix, (unsigned int)(id), i);
                    if (uid == NULL)
                        return NULL;

                    if (!find_node_by_uid(uid))
                        return uid;
                }
                return NULL;
            }

            status_t registry::process_add(uint32_t id, uint32_t permissions, const char *type, uint32_t version, const spa_dict *props) noexcept
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
                    if ((node->sUID = make_unique_id(uid_seed, id)) == NULL)
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

                    const bool is_monitor   = fetch_pw_bool(props, PW_KEY_PORT_MONITOR);

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
                    port->sSystemId         = strfmt(
                        "%s_%u",
                        ((flags & PORT_DIR_MASK) == PORT_DIR_IN) ? "playback" : ((is_monitor) ? "monitor" : "capture"),
                        (unsigned)(port_id + 1));
                    if (port->sSystemId == NULL)
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
                        (out_node != NULL) ? out_node->sName : "<null>", int(link->nOutNodeID),
                        (out_port != NULL) ? out_port->sName : "<null>", int(link->nOutPortID),
                        (in_node != NULL) ? in_node->sName : "<null>", int(link->nInNodeID),
                        (in_port != NULL) ? in_port->sName : "<null>", int(link->nInPortID));
                #endif /* LSP_TRACE */
                    link = NULL;
                }

                return STATUS_OK;
            }

            status_t registry::process_metadata(uint32_t id, const char *key, const char *type, const char *value) noexcept
            {
                if (id == PW_ID_CORE)
                {
                    if ((key == NULL) || (strcmp(key, PW_KEY_DEFAULT_AUDIO_SINK) == 0))
                    {
                        char * const name = lsp::exchange(sDefaultSink, fetch_spa_string(value, PW_KEY_NAME));
                        if (name != NULL)
                            free(name);
                        lsp_trace("Default audio sink set to %s", sDefaultSink);
                    }
                    if ((key == NULL) || (strcmp(key, PW_KEY_DEFAULT_AUDIO_SOURCE) == 0))
                    {
                        char * const name = lsp::exchange(sDefaultSource, fetch_spa_string(value, PW_KEY_NAME));
                        if (name != NULL)
                            free(name);
                        lsp_trace("Default audio source set to %s", sDefaultSource);
                    }
                }

                return STATUS_OK;
            }

            status_t registry::process_remove(uint32_t id) noexcept
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
                        (out_node != NULL) ? out_node->sName : "<null>", int(link->nOutNodeID),
                        (out_port != NULL) ? out_port->sName : "<null>", int(link->nOutPortID),
                        (in_node != NULL) ? in_node->sName : "<null>", int(link->nInNodeID),
                        (in_port != NULL) ? in_port->sName : "<null>", int(link->nInPortID));
                #endif /* LSP_TRACE */
                    destroy(link);
                    return STATUS_OK;
                }

                return STATUS_NOT_FOUND;
            }

            const node_t *registry::find_node_by_id(uint32_t id) const noexcept
            {
                registry * const self = const_cast<registry *>(this);
                return find_by_id<node_t>(self->vNodes, id);
            }

            const node_t *registry::find_node_by_name(const char *name) const noexcept
            {
                for (size_t i=0, n=vNodes.nCount; i<n; ++i)
                {
                    const node_t * const node = static_cast<const node_t *>(vNodes.vObjects[i]);
                    if ((node->sName != NULL) && (strcmp(node->sName, name) == 0))
                        return node;
                }
                return NULL;
            }

            const node_t *registry::find_node_by_uid(const char *uid) const noexcept
            {
                for (size_t i=0, n=vNodes.nCount; i<n; ++i)
                {
                    const node_t * const node = static_cast<const node_t *>(vNodes.vObjects[i]);
                    if ((node->sUID != NULL) && (strcmp(node->sUID, uid) == 0))
                        return node;
                }
                return NULL;
            }

            const node_t *registry::find_node_by_nick(const char *name) const noexcept
            {
                for (size_t i=0, n=vNodes.nCount; i<n; ++i)
                {
                    const node_t * const node = static_cast<const node_t *>(vNodes.vObjects[i]);
                    if ((node->sNick != NULL) && (strcmp(node->sNick, name) == 0))
                        return node;
                }
                return NULL;
            }

            const node_t *registry::find_node_by_string(const char *name) const noexcept
            {
                const node_t * node = find_node_by_uid(name);
                if (node != NULL)
                    return node;
                node = find_node_by_name(name);
                if (node != NULL)
                    return node;
                return find_node_by_nick(name);
            }

            const port_t *registry::find_node_port(uint32_t node_id, const char *port_id, uint32_t direction) const noexcept
            {
                // Lookup by port name first
                for (size_t i=0, n=vPorts.nCount; i<n; ++i)
                {
                    const port_t * const port = static_cast<const port_t *>(vPorts.vObjects[i]);
                    if (port->nNodeID != node_id)
                        continue;
                    if (((port->nFlags ^ direction) & PORT_DIR_MASK) != 0)
                        continue;
                    if (strcmp(port->sName, port_id) == 0)
                        return port;
                }

                // Lookup by system port name next
                for (size_t i=0, n=vPorts.nCount; i<n; ++i)
                {
                    const port_t * const port = static_cast<const port_t *>(vPorts.vObjects[i]);
                    if (port->nNodeID != node_id)
                        continue;
                    if (((port->nFlags ^ direction) & PORT_DIR_MASK) != 0)
                        continue;
                    if (strcmp(port->sSystemId, port_id) == 0)
                        return port;
                }
                return NULL;
            }

            const port_t *registry::find_port_by_id(uint32_t id) const noexcept
            {
                registry * const self = const_cast<registry *>(this);
                return find_by_id<port_t>(self->vPorts, id);
            }

            const port_t *registry::find_port(const char *port_id, uint32_t direction) const noexcept
            {
                // Find the port name separator character
                const char * const sep = strrchr(const_cast<char *>(port_id), ':');
                if (sep == NULL)
                    return NULL;
                const size_t sep_index = sep - port_id;

                // Make copy of the string to replace separator character with '\0'
                char * const node_id    = strdup(port_id);
                if (node_id == NULL)
                    return NULL;
                lsp_finally { free(node_id); };

                char * const port_name  = &node_id[sep_index + 1];
                node_id[sep_index]      = '\0';

                // Determine the name of the node to lookup
                const char *lookup_id   = node_id;
                if (strcmp(node_id, DEFAULT_DEVICE_NAME) == 0)
                {
                    lookup_id   = ((direction & PORT_DIR_MASK) == PORT_DIR_IN) ? sDefaultSink : sDefaultSource;
                    if (lookup_id == NULL)
                        lookup_id   = node_id;
                }

                // Lookup the node
                const node_t * const node   = find_node_by_string(lookup_id);
                return (node != NULL) ? find_node_port(node->nID, port_name, direction) : NULL;
            }

            const link_t *registry::find_link(const port_t *src, const port_t *dst) const noexcept
            {
                if ((src == NULL) || (dst == NULL))
                    return NULL;
                return find_link(src->nNodeID, src->nID, dst->nNodeID, dst->nID);
            }

            const link_t *registry::find_link(uint32_t src_node_id, uint32_t src_port_id, uint32_t dst_node_id, uint32_t dst_port_id) const noexcept
            {
                // Lookup by port name first
                for (size_t i=0, n=vLinks.nCount; i<n; ++i)
                {
                    const link_t * const link = static_cast<const link_t *>(vLinks.vObjects[i]);
                    if ((link->nOutNodeID == src_node_id) &&
                        (link->nOutPortID == src_port_id) &&
                        (link->nInNodeID == dst_node_id) &&
                        (link->nInPortID == dst_port_id))
                        return link;
                }

                return NULL;
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


