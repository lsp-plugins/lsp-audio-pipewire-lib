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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MUTEX_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MUTEX_H_

#include <lsp-plug.in/audio/pipewire/mutex.h>
#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/types.h>

#ifdef PLATFORM_POSIX
    #include <pthread.h>
    #include <errno.h>
#else
    #error "Need to implement mutex for the target platform"
#endif /* PLATFORM_POSIX*/

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            typedef struct mutex_t
            {
                pthread_mutex_t lock;
            } mutex_t;

            inline mutex_t *mutex_create()
            {
                mutex_t *mutex = static_cast<mutex_t *>(malloc(sizeof(pthread_mutex_t)));
                if (mutex == NULL)
                    return NULL;
                lsp_finally {
                    if (mutex != NULL)
                        free(mutex);
                };

                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
                pthread_mutex_init(&mutex->lock, &attr);
                pthread_mutexattr_destroy(&attr);

                return release_ptr(mutex);
            }

            inline void mutex_destroy(mutex_t *mutex)
            {
                if (mutex == NULL)
                    return;

                pthread_mutex_destroy(&mutex->lock);
                free(mutex);
            }

            inline void mutex_lock(mutex_t *mutex)
            {
                if (mutex != NULL)
                    pthread_mutex_lock(&mutex->lock);
            }

            inline bool mutex_trylock(mutex_t *mutex)
            {
                if (mutex == NULL)
                    return true;
                return pthread_mutex_trylock(&mutex->lock) != EBUSY;
            }

            inline void mutex_unlock(mutex_t *mutex)
            {
                if (mutex != NULL)
                    pthread_mutex_unlock(&mutex->lock);
            }

            inline mutex_guard::mutex_guard(mutex_t *mutex)
            {
                pMutex = mutex;
                mutex_lock(pMutex);
            }

            inline mutex_guard::mutex_guard(const mutex_guard & src)
            {
                pMutex = src.pMutex;
                mutex_lock(pMutex);
            }

            inline mutex_guard::mutex_guard(mutex_guard && src)
            {
                pMutex = lsp::exchange(src.pMutex, static_cast<mutex_t *>(NULL));
            }

            inline mutex_guard::~mutex_guard()
            {
                mutex_unlock(pMutex);
                pMutex = NULL;
            }

            mutex_guard & mutex_guard::operator = (const mutex_guard & src)
            {
                mutex_lock(src.pMutex);
                mutex_unlock(pMutex);
                pMutex = src.pMutex;
                return *this;
            }

            inline mutex_guard & mutex_guard::operator = (mutex_guard && src)
            {
                pMutex = lsp::exchange(src.pMutex, static_cast<mutex_t *>(NULL));
                return *this;
            }

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_IMPL_MUTEX_H_ */
