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
#include <lsp-plug.in/audio/iface/builtin.h>
#include <lsp-plug.in/audio/pipewire/factory.h>

#ifndef LSP_IDE_DEBUG

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            extern "C"
            {
                // Function that returns factory
                LSP_AUDIO_PIPEWIRE_LIB_PUBLIC
                LSP_AUDIO_BULTIN_FACTORY_FUNCTION
            }
        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */

#endif /* LSP_IDE_DEBUG */
