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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_MUTEX_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_MUTEX_H_

#include <lsp-plug.in/audio/pipewire/version.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            struct mutex_t;

            /**
             * Create mutex
             * @return pointer to mutex or NULL
             */
            inline mutex_t     *mutex_create();

            /**
             * Destroy mutex
             * @param mutex pointer to mutex
             */
            inline void         mutex_destroy(mutex_t *mutex);

            /**
             * Lock mutex
             * @param mutex pointer to mutex
             */
            inline void         mutex_lock(mutex_t *mutex);

            /**
             * Ty to lock mutex
             * @param mutex pointer to mutex
             * @return true if mutex has been locked
             */
            inline bool         mutex_trylock(mutex_t *mutex);

            /**
             * Unlock mutex
             * @param mutex mutex to unlock
             */
            inline void         mutex_unlock(mutex_t *mutex);

            /**
             * Mutex guard
             */
            struct mutex_guard
            {
                private:
                    mutex_t        *pMutex;

                public:
                    explicit inline mutex_guard(mutex_t *mutex);
                    inline mutex_guard(const mutex_guard & src);
                    inline mutex_guard(mutex_guard && src);
                    inline ~mutex_guard();

                    inline mutex_guard & operator = (const mutex_guard & src);
                    inline mutex_guard & operator = (mutex_guard && src);
            };

            #define MUTEX_SCOPED_LOCK__2(mutex, counter) mutex_guard guard ## counter (mutex)
            #define MUTEX_SCOPED_LOCK__1(mutex, counter) MUTEX_SCOPED_LOCK__2(mutex, counter)
            #define MUTEX_SCOPED_LOCK(mutex) MUTEX_SCOPED_LOCK__1(mutex, __COUNTER__)

        } /* namespace pipewire */
    }  /* namespace audio */
}  /* namespace lsp */

#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_MUTEX_H_ */
