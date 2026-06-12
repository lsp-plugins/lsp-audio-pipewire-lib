/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 7 мая 2026 г.
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

#ifndef LSP_PLUG_IN_AUDIO_PIPEWIRE_MESSAGES_H_
#define LSP_PLUG_IN_AUDIO_PIPEWIRE_MESSAGES_H_

#include <lsp-plug.in/audio/pipewire/version.h>

#include <lsp-plug.in/audio/pipewire/ringbuffer.h>
#include <lsp-plug.in/common/types.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            constexpr uint16_t MESSAGE_MAGIC = 0x5254;

            enum message_type_t
            {
                MSG_UNKNOWN,            // Unknown message
                MSG_LATENCY,            // Latency message
            };

            /**
             * Common mesage header
             */
            typedef struct header_t
            {
                uint16_t    magic;      // Magic header
                uint16_t    type;       // type of the message
                uint16_t    length;     // Length of the whole message
                uint16_t    cksum;      // Checksum of the message
            } header_t;

            /**
             * Latency change message
             */
            typedef struct latency_t
            {
                uint32_t    latency;    // Message latency
            } latency_t;

            /**
             * Common message which can be fetched from ring buffer
             */
            typedef struct message_t
            {
                header_t    header;
                union
                {
                    uint8_t     data[4];
                    latency_t   latency;
                };
            } message_t;

            /**
             * Initialize message header
             * @param header message header
             * @param type message type
             * @param length the payload length
             */
            void init_header(header_t *header, uint16_t type, uint16_t length) noexcept;

            /**
             * Validate message header
             * @param header message header to validate
             * @return zero value on valid checksum
             */
            uint16_t header_checksum(const header_t *header) noexcept;

            /**
             * Read the whole message from ring buffer
             * @param message message to read
             * @param buffer buffer to read data
             * @return status of operation
             */
            status_t read_message(message_t *message, ringbuffer *buffer) noexcept;

            /**
             * Write the whole message to the ring buffer
             * @param buffer buffer to read data
             * @param message message to read
             * @return status of operation
             */
            status_t write_message(ringbuffer *bufer, const message_t *message) noexcept;

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


#endif /* LSP_PLUG_IN_AUDIO_PIPEWIRE_MESSAGES_H_ */
