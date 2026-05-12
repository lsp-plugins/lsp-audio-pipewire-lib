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
    #include <errno.h>
    #include <pthread.h>
    #include <sys/time.h>
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
                pthread_cond_t cond;
            } mutex_t;

            inline mutex_t *mutex_create()
            {
                mutex_t *mutex = malloc_count<mutex_t>(1);
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

                pthread_condattr_t cond_attr;
                pthread_condattr_init(&cond_attr);
                pthread_cond_init(&mutex->cond, &cond_attr);
                pthread_condattr_destroy(&cond_attr);

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

            status_t mutex_wait(mutex_t *mutex, uint32_t millis)
            {
                struct timeval now;
                struct timespec deadline;

                // Set-up the fire time
                gettimeofday(&now, NULL);
                deadline.tv_nsec    = now.tv_usec * 1000 + (millis % 1000) * 1000000;
                deadline.tv_sec     = now.tv_sec + (millis / 1000) + (deadline.tv_nsec / 1000000000);
                deadline.tv_nsec   %= 1000000000;

                // Perform wait
                int result          = pthread_cond_timedwait(&mutex->cond, &mutex->lock, &deadline);
                switch (result)
                {
                    case 0: return STATUS_OK;
                    case ETIMEDOUT: return STATUS_TIMED_OUT;
                    case EPERM: return STATUS_BAD_STATE;
                    default: break;
                }

                return STATUS_UNKNOWN_ERR;
            }

            bool mutex_notify(mutex_t *mutex)
            {
                return pthread_cond_broadcast(&mutex->cond) == 0;
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
