/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 2 мая 2026 г.
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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_PW_DEFS_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_PW_DEFS_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <pipewire/version.h>
#include <pipewire/keys.h>
#include <spa/node/io.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef SPA_KEY_THREAD_RESET_ON_FORK
    #define SPA_KEY_THREAD_RESET_ON_FORK    "thread.reset-on-fork"  /* reset priority and policy for real-time threads on fork. Default true */
#endif /* SPA_KEY_THREAD_RESET_ON_FORK */

#ifndef PW_KEY_LOOP_CANCEL
    #define PW_KEY_LOOP_CANCEL              "loop.cancel"
#endif /* PW_KEY_LOOP_CANCEL */

#if !PW_CHECK_VERSION(1,1,81)

#define SPA_IO_AsyncBuffers                 10

struct spa_io_async_buffers
{
    struct spa_io_buffers buffers[2];
};

#endif /* !PW_CHECK_VERSION */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_PW_DEFS_H_ */
