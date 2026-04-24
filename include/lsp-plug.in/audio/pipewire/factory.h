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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_FACTORY_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_FACTORY_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/iface/factory.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            // JACK audio backend factory
            typedef struct LSP_HIDDEN_MODIFIER factory_t: public audio::factory_t
            {
                static const audio::backend_metadata_t      sMetadata[];

                explicit factory_t();
                ~factory_t();

                static const audio::backend_metadata_t      *metadata(audio::factory_t *self, size_t id);
                static audio::backend_t                     *create(audio::factory_t *self, size_t id);

            } factory_t;
        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_FACTORY_H_ */
