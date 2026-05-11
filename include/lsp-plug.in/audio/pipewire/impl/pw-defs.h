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

#include <pw-headers/pipewire/version.h>
#include <pw-headers/pipewire/keys.h>
#include <pw-headers/spa/node/io.h>
#include <pw-headers/spa/pod/pod.h>

#ifndef SPA_KEY_THREAD_RESET_ON_FORK
    #define SPA_KEY_THREAD_RESET_ON_FORK    "thread.reset-on-fork"  /* reset priority and policy for real-time threads on fork. Default true */
#endif /* SPA_KEY_THREAD_RESET_ON_FORK */

#ifndef PW_KEY_CONTROL_UMP
    #define PW_KEY_CONTROL_UMP              "control.ump"
#endif /* PW_KEY_CONTROL_UMP */

#ifndef PW_KEY_LOOP_CANCEL
    #define PW_KEY_LOOP_CANCEL              "loop.cancel"
#endif /* PW_KEY_LOOP_CANCEL */

#ifndef PW_KEY_DEFAULT_AUDIO_SINK
    #define PW_KEY_DEFAULT_AUDIO_SINK       "default.audio.sink"
#endif /* PW_KEY_DEFAULT_AUDIO_SINK */

#ifndef PW_KEY_DEFAULT_AUDIO_SOURCE
    #define PW_KEY_DEFAULT_AUDIO_SOURCE     "default.audio.source"
#endif /* PW_KEY_DEFAULT_AUDIO_SOURCE */

#ifndef PW_PORT_DIRECTION_IN
    #define PW_PORT_DIRECTION_IN            "in"
#endif /* PW_PORT_DIRECTION_IN */

#ifndef PW_PORT_DIRECTION_OUT
    #define PW_PORT_DIRECTION_OUT           "out"
#endif /* PW_PORT_DIRECTION_OUT */

#ifndef PW_KEY_NAME
    #define PW_KEY_NAME                     "name"
#endif /* PW_KEY_NAME */

#ifndef PW_KEY_DEFAULT_CLOCK_QUANTUM
    #define PW_KEY_DEFAULT_CLOCK_QUANTUM    "default.clock.quantum"
#endif /* PW_KEY_DEFAULT_CLOCK_QUANTUM */

#ifndef PW_KEY_DEFAULT_CLOCK_QUANTUM_LIMIT
    #define PW_KEY_DEFAULT_CLOCK_QUANTUM_LIMIT  "default.clock.quantum-limit"
#endif /* PW_KEY_DEFAULT_CLOCK_QUANTUM_LIMIT */

#ifndef PW_KEY_DEFAULT_CLOCK_RATE
    #define PW_KEY_DEFAULT_CLOCK_RATE       "default.clock.rate"
#endif /* PW_KEY_DEFAULT_CLOCK_RATE */

#ifndef PW_KEY_CLIENT_RULES
    #define PW_KEY_CLIENT_RULES             "client.rules"
#endif /* PW_KEY_CLIENT_RULES */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_PW_DEFS_H_ */
