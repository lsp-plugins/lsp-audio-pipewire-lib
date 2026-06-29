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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/pipewire/registry_types.h>
#include <lsp-plug.in/common/status.h>

#include <pw-headers/pipewire/core.h>
#include <pw-headers/spa/utils/dict.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            /**
             * The reigstry for pipewire objects
             */
            struct registry
            {
                private:
                    typedef struct storage_t
                    {
                        object_t      **vObjects;
                        uint32_t        nCount;
                        uint32_t        nCapacity;
                    } storage_t;

                protected:
                    char           *sDefaultSource;
                    char           *sDefaultSink;
                    storage_t       vDevices;
                    storage_t       vClients;
                    storage_t       vNodes;
                    storage_t       vPorts;
                    storage_t       vLinks;

                protected:
                    static inline void  init_storage(storage_t & storage) noexcept;
                    template <typename T>
                    static inline void  destroy_storage(storage_t & storage) noexcept;
                    template <typename T>
                    static inline T    *find_by_id(storage_t & storage, uint32_t id) noexcept;
                    static object_t    *find_object_by_id(storage_t & storage, uint32_t id) noexcept;
                    static uint32_t     storage_index_of(storage_t & storage, uint32_t id) noexcept;
                    static bool         store_object(storage_t & storage, object_t * & old, object_t *object) noexcept;
                    template <typename T>
                    static bool         add_to_storage(storage_t & storage, T *object) noexcept;
                    template <typename T>
                    static inline T    *remove_by_id(storage_t & storage, uint32_t id) noexcept;

                    static inline void destroy(device_t *item) noexcept;
                    static inline void destroy(client_t *item) noexcept;
                    static inline void destroy(node_t *item) noexcept;
                    static inline void destroy(port_t *item) noexcept;
                    static inline void destroy(link_t *item) noexcept;

                    static inline device_t     *alloc_device() noexcept;
                    static inline client_t     *alloc_client() noexcept;
                    static inline node_t       *alloc_node() noexcept;
                    static inline port_t       *alloc_port() noexcept;
                    static inline link_t       *alloc_link() noexcept;

                protected:
                    char           *make_unique_id(const char *prefix, uint32_t id) noexcept;

                public:
                    registry() noexcept;
                    registry(const registry &) = delete;
                    registry(registry && src) = delete;
                    ~registry() noexcept;

                    registry & operator = (const registry &) = delete;
                    registry & operator = (registry && src) = delete;

                    void            construct() noexcept;
                    void            destroy() noexcept;

                public: // PipeWire management interface
                    status_t        process_add(uint32_t id, uint32_t permissions, const char *type, uint32_t version, const spa_dict *props) noexcept;
                    status_t        process_metadata(uint32_t id, const char *key, const char *type, const char *value) noexcept;
                    status_t        process_remove(uint32_t id) noexcept;

                public:
                    const node_t   *find_node_by_id(uint32_t id) const noexcept;
                    const node_t   *find_node_by_name(const char *name) const noexcept;
                    const node_t   *find_node_by_nick(const char *name) const noexcept;
                    const node_t   *find_node_by_string(const char *name) const noexcept;
                    const node_t   *find_node_by_uid(const char *uid) const noexcept;
                    const port_t   *find_node_port(uint32_t node_id, const char *port_id, uint32_t direction) const noexcept;

                    const port_t   *find_port_by_id(uint32_t id) const noexcept;
                    const port_t   *find_port(const char *port_id, uint32_t direction) const noexcept;

                    const link_t   *find_link(const port_t *src, const port_t *dst) const noexcept;
                    const link_t   *find_link(uint32_t src_node_id, uint32_t src_port_id, uint32_t dst_node_id, uint32_t dst_port_id) const noexcept;
            };
        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_H_ */
