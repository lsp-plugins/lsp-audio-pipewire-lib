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

#include <lsp-plug.in/audio/pipewire/dictionary.h>

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/stdlib/string.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            prop_dict::prop_dict() noexcept
            {
                construct();
            }

            prop_dict::~prop_dict() noexcept
            {
                destroy();
            }

            void prop_dict::construct() noexcept
            {
                sData.flags     = SPA_DICT_FLAG_SORTED;
                sData.n_items   = 0;
                sData.items     = NULL;
                nCapacity       = 0;
            }

            void prop_dict::destroy() noexcept
            {
                if (sData.items != NULL)
                {
                    spa_dict_item *items = const_cast<spa_dict_item *>(sData.items);
                    sData.items     = NULL;

                    for (size_t i=0, n=sData.n_items; i<n; ++i)
                    {
                        spa_dict_item * const item = &items[i];
                        if (item->key != NULL)
                            free(const_cast<char *>(item->key));
                        if (item->value != NULL)
                            free(const_cast<char *>(item->value));
                    }

                    free(items);
                }
                sData.n_items   = 0;
                nCapacity       = 0;
            }

            void prop_dict::swap(prop_dict *dst) noexcept
            {
                lsp::swap(sData, dst->sData);
                lsp::swap(nCapacity, dst->nCapacity);
            }

            void prop_dict::swap(prop_dict &dst) noexcept
            {
                lsp::swap(sData, dst.sData);
                lsp::swap(nCapacity, dst.nCapacity);
            }

            uint32_t prop_dict::index_of(const char *key) const noexcept
            {
                ssize_t first = 0, last = ssize_t(sData.n_items) - 1;
                while (first <= last)
                {
                    const ssize_t middle = (first + last) >> 1;
                    const spa_dict_item * const item = &sData.items[middle];
                    const int cmp       = strcmp(key, item->key);

                    if (cmp < 0)
                        last                = middle - 1;
                    else if (cmp > 0)
                        first               = middle + 1;
                    else
                        return middle;
                }

                return uint32_t(first);
            }

            status_t prop_dict::ensure_capacity() noexcept
            {
                if ((sData.n_items + 1) <= nCapacity)
                    return STATUS_OK;

                const size_t new_cap        = lsp_max(nCapacity + (nCapacity >> 1), 16u);
                spa_dict_item * const items = static_cast<spa_dict_item *>(realloc(const_cast<spa_dict_item *>(sData.items), new_cap));
                if (items == NULL)
                    return STATUS_NO_MEM;

                sData.items             = items;
                return STATUS_OK;
            }

            status_t prop_dict::append(const char *key, const char *value) noexcept
            {
                // Ensure that we have enough space to add one more item
                const status_t res = ensure_capacity();
                if (res != STATUS_OK)
                    return res;

                // Make copy of key and value
                char *pkey      = strdup(key);
                if (pkey == NULL)
                    return STATUS_NO_MEM;
                char *pvalue    = strdup(value);
                if (pvalue == NULL)
                {
                    free(pkey);
                    return STATUS_NO_MEM;
                }

                // Store key and value
                spa_dict_item * const item = const_cast<spa_dict_item *>(&sData.items[sData.n_items]);
                item->key       = pkey;
                item->value     = pvalue;

                return STATUS_OK;
            }

            status_t prop_dict::insert_at(size_t index, const char *key, const char *value) noexcept
            {
                // Ensure that we have enough space to add one more item
                const status_t res = ensure_capacity();
                if (res != STATUS_OK)
                    return res;

                // Make copy of key and value
                char *pkey      = strdup(key);
                if (pkey == NULL)
                    return STATUS_NO_MEM;
                char *pvalue    = strdup(value);
                if (pvalue == NULL)
                {
                    free(pkey);
                    return STATUS_NO_MEM;
                }

                // Reserve space for the item
                spa_dict_item * const items = const_cast<spa_dict_item *>(sData.items);
                memmove(&items[index + 1], &items[index], (sData.n_items - index) * sizeof(spa_dict_item));

                // Store key and value
                spa_dict_item * const item = &items[index];
                item->key       = pkey;
                item->value     = pvalue;

                return STATUS_OK;
            }

            status_t prop_dict::put(const char *key, const char *value) noexcept
            {
                if ((key == NULL) || (value == NULL))
                    return STATUS_BAD_ARGUMENTS;

                // Append value if out of bounds
                const uint32_t items = sData.n_items;
                const uint32_t index = index_of(key);
                if (index >= items)
                    return append(key, value);

                // Insert value at some position
                const int cmp_result = strcmp(key, sData.items[index].key);
                if (cmp_result < 0)
                    return insert_at(index, key, value);
                else if (cmp_result > 0)
                {
                    const uint32_t new_index = index + 1;
                    return (new_index >= items) ?
                        append(key, value) :
                        insert_at(index, key, value);
                }

                // Make copy of value
                char *pvalue    = strdup(value);
                if (pvalue == NULL)
                    return STATUS_NO_MEM;

                // Just update value
                spa_dict_item * const item = const_cast<spa_dict_item *>(&sData.items[index]);
                if (item->value != NULL)
                    free(const_cast<char *>(item->value));
                item->value    = pvalue;

                return STATUS_OK;
            }

            status_t prop_dict::put(const spa_dict_item *item) noexcept
            {
                return (item != NULL) ? put(item->key, item->value) : STATUS_BAD_ARGUMENTS;
            }

            status_t prop_dict::put(const pw_properties *props) noexcept
            {
                if (props == NULL)
                    return STATUS_BAD_ARGUMENTS;

                void *state = NULL;
                const char *key = NULL;
                while ((key = pw_properties_iterate(props, &state)) != NULL)
                {
                    const char * const value = pw_properties_get(props, key);
                    if (value == NULL)
                        return STATUS_BAD_ARGUMENTS;

                    status_t res = put(key, value);
                    if (res != STATUS_OK)
                        return res;
                }

                return STATUS_OK;
            }

            status_t prop_dict::put(const spa_dict *props) noexcept
            {
                if (props == NULL)
                    return STATUS_BAD_ARGUMENTS;

                for (size_t i=0, n=props->n_items; i<n; ++i)
                {
                    status_t res = put(&props->items[i]);
                    if (res != STATUS_OK)
                        return res;
                }

                return STATUS_OK;
            }

            status_t prop_dict::put(const prop_dict *props) noexcept
            {
                return (props != NULL) ? put(props->dict()) : STATUS_BAD_ARGUMENTS;
            }

            status_t prop_dict::set(const pw_properties *props) noexcept
            {
                prop_dict tmp;
                const status_t res = tmp.put(props);
                if (res == STATUS_OK)
                    tmp.swap(this);

                return res;
            }

            status_t prop_dict::set(const spa_dict *props) noexcept
            {
                prop_dict tmp;
                const status_t res = tmp.put(props);
                if (res == STATUS_OK)
                    tmp.swap(this);

                return res;
            }

            status_t prop_dict::set(const prop_dict *props) noexcept
            {
                return (props != NULL) ? set(props->dict()) : STATUS_BAD_ARGUMENTS;
            }

            status_t prop_dict::put(spa_dict_item item) noexcept
            {
                return put(item.key, item.value);
            }

            const spa_dict_item *prop_dict::item(const char *key) const noexcept
            {
                ssize_t first = 0, last = ssize_t(sData.n_items) - 1;
                while (first <= last)
                {
                    const ssize_t middle = (first + last) >> 1;
                    const spa_dict_item * const item = &sData.items[middle];
                    const int cmp       = strcmp(key, item->key);

                    if (cmp < 0)
                        last                = middle - 1;
                    else if (cmp > 0)
                        first               = middle + 1;
                    else
                        return item;
                }

                return NULL;
            }

            const char *prop_dict::value(const char *key, const char *dfl) const noexcept
            {
                const spa_dict_item * const v = item(key);
                return (v != NULL) ? v->value : dfl;
            }

            bool prop_dict::exists(const char *key) const noexcept
            {
                const spa_dict_item * const v = item(key);
                return v != NULL;
            }

            const char *prop_dict::key(size_t index) const noexcept
            {
                return (index < sData.n_items) ? sData.items[index].key : NULL;
            }

            const char *prop_dict::value(size_t index) const noexcept
            {
                return (index < sData.n_items) ? sData.items[index].value : NULL;
            }

            const spa_dict_item *prop_dict::item(size_t index) const noexcept
            {
                return (index < sData.n_items) ? &sData.items[index] : NULL;
            }

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


