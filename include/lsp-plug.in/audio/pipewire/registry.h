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

#include <spa/utils/dict.h>

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

                    static inline void destroy(client_t *iteme) noexcept;
                    static inline void destroy(node_t *item) noexcept;
                    static inline void destroy(port_t *item) noexcept;
                    static inline void destroy(link_t *item) noexcept;

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
                    status_t        process_remove(uint32_t id) noexcept;

                public:
                    const node_t   *find_node_by_id(uint32_t id) const;
                    const node_t   *find_node_by_name(const char *name) const;
                    const node_t   *find_node_by_uid(const char *uid) const;
            };
        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_REGISTRY_H_ */
