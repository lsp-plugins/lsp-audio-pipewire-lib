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

#include <lsp-plug.in/common/types.h>
#include <lsp-plug.in/audio/pipewire/backend.h>
#include <lsp-plug.in/audio/pipewire/factory.h>

#include <lsp-plug.in/common/static.h>

#include <pipewire/pipewire.h>
#include <stdlib.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            const audio::backend_metadata_t factory_t::sMetadata[] =
            {
                {
                    "pipewire",
                    "PipeWire Audio Backend",
                    "pipewire",
                    90
                }
            };

            // Static initialization of PipeWire
            static void init_pipewire()
            {
                pw_init(NULL, NULL);
            }

            static StaticInitializer pipewire_initializer(init_pipewire);


            const audio::backend_metadata_t *factory_t::metadata(audio::factory_t *self, size_t id)
            {
                const size_t count = sizeof(sMetadata) / sizeof(audio::backend_metadata_t);
                return (id < count) ? &sMetadata[id] : NULL;
            }

            audio::backend_t *factory_t::create(audio::factory_t *self, size_t id)
            {
                if (id == 0)
                {
                    pipewire::backend_t * const res = static_cast<pipewire::backend_t *>(::malloc(sizeof(pipewire::backend_t)));
                    if (res != NULL)
                        res->construct();
                    return res;
                }
                return NULL;
            }

            factory_t::factory_t()
            {
                #define AUDIO_PIPEWIRE_FACTORY_EXP(func)   audio::factory_t::func   = pipewire::factory_t::func;
                AUDIO_PIPEWIRE_FACTORY_EXP(create);
                AUDIO_PIPEWIRE_FACTORY_EXP(metadata);
                #undef AUDIO_PIPEWIRE_FACTORY_EXP
            }

            factory_t::~factory_t()
            {
            }
        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */



