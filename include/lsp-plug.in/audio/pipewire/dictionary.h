/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 26 апр. 2026 г.
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_DICTIONARY_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_DICTIONARY_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/common/variadic.h>
#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/common/types.h>

#include <pipewire/properties.h>
#include <spa/utils/dict.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            typedef struct dictionary
            {
                protected:
                    spa_dict        sData;
                    uint32_t        nCapacity;
                    mutable char   *sToString;

                protected:
                    uint32_t                    index_of(const char *key) const noexcept;
                    status_t                    ensure_capacity() noexcept;
                    status_t                    append(const char *key, const char *value) noexcept;
                    status_t                    insert_at(size_t index, const char *key, const char *value) noexcept;

                public:
                    dictionary() noexcept;
                    dictionary(const dictionary &) = delete;
                    dictionary(dictionary &&) = delete;
                    dictionary & operator = (const dictionary &) = delete;
                    dictionary & operator = (dictionary &&) = delete;
                    ~dictionary() noexcept;

                    void                        construct() noexcept;
                    void                        destroy() noexcept;
                    void                        swap(dictionary *dst) noexcept;
                    void                        swap(dictionary &dst) noexcept;

                public:
                    status_t                    set(const pw_properties *props) noexcept;
                    status_t                    set(const spa_dict *props) noexcept;
                    status_t                    set(const dictionary *props) noexcept;

                    status_t                    put(const char *key, const char *value) noexcept;
                    status_t                    put(const spa_dict_item *item) noexcept;
                    status_t                    put(spa_dict_item item) noexcept;
                    status_t                    put(const pw_properties *props) noexcept;
                    status_t                    put(const spa_dict *props) noexcept;
                    status_t                    put(const dictionary *props) noexcept;
                    const char                 *value(const char *key, const char *dfl = NULL) const noexcept;
                    const spa_dict_item        *item(const char *key) const noexcept;
                    const char                 *key(size_t index) const noexcept;
                    const char                 *value(size_t index) const noexcept;
                    const spa_dict_item        *item(size_t index) const noexcept;
                    bool                        exists(const char *key) const noexcept;
                    const char                 *to_string() const noexcept;

                public:
                    inline const spa_dict      *dict() const noexcept           { return &sData;                }
                    inline const spa_dict_item *items() const noexcept          { return sData.items;           }
                    inline size_t               size() const noexcept           { return sData.n_items;         }
                    inline size_t               capacity() const noexcept       { return nCapacity;             }
                    inline pw_properties       *make_properties() const noexcept{ return pw_properties_new_dict(&sData);    }
                    inline bool                 contains(const char *key) const noexcept { return exists(key);  }
                    inline bool                 is_empty() const noexcept       { return sData.n_items == 0;    }
                    inline void                 clear() noexcept                { destroy();                    }

                public:
                    template <typename ... A>
                    inline status_t             put(const char *key, const char *value, A && ... args) noexcept
                    {
                        const status_t res = put(key, value);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }

                    template <typename ... A>
                    inline status_t             put(const spa_dict_item *item, A && ... args) noexcept
                    {
                        const status_t res = put(item);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }

                    template <typename ... A>
                    inline status_t             put(spa_dict_item item, A && ... args) noexcept
                    {
                        const status_t res = put(item);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }

                    template <typename ... A>
                    inline status_t             put(const pw_properties *props, A && ... args) noexcept
                    {
                        const status_t res = put(props);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }

                    template <typename ... A>
                    inline status_t             put(const spa_dict *props, A && ... args) noexcept
                    {
                        const status_t res = put(props);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }

                    template <typename ... A>
                    inline status_t             put(const dictionary *props, A && ... args) noexcept
                    {
                        const status_t res = put(props);
                        return (res == STATUS_OK) ? put(lsp::forward<A>(args)...) : res;
                    }
            } dictionary;

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_DICTIONARY_H_ */
