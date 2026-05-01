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

#include <lsp-plug.in/audio/pipewire/backend.h>
#include <pipewire/proxy.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
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

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_CAST_H_ */
