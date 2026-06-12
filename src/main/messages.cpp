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

#include <lsp-plug.in/audio/pipewire/messages.h>

namespace lsp
{
    namespace audio
    {
        namespace pipewire
        {
            uint16_t header_checksum(const header_t *header) noexcept
            {
                return
                    header->magic ^
                    header->type ^
                    (header->length >> 8) ^
                    (header->length << 8) ^
                    header->cksum;
            }

            void init_header(header_t *header, uint16_t type, uint16_t length) noexcept
            {
                header->magic   = MESSAGE_MAGIC;
                header->type    = type;
                header->length  = sizeof(header_t) + length;
                header->cksum   = 0;
                header->cksum   = header_checksum(header);
            }

            status_t read_message(message_t *message, ringbuffer *buffer) noexcept
            {
                // Read message header and validate
                ssize_t count   = buffer->read(&message->header, sizeof(header_t));
                if (count < 0)
                    return status_t(-count);

                if (header_checksum(&message->header) != 0)
                    return STATUS_CORRUPTED;
                const size_t length = message->header.length;
                if ((length < sizeof(header_t)) || (length > sizeof(message_t)))
                    return STATUS_CORRUPTED;

                // Read message body if necessary
                if (length > sizeof(header_t))
                {
                    count           = buffer->read(message->data, length - sizeof(header_t));
                    if (count < 0)
                        return (count == -STATUS_NO_DATA) ? status_t(-count) : STATUS_CORRUPTED;
                }

                return STATUS_OK;
            }

            status_t write_message(ringbuffer *buffer, const message_t *message) noexcept
            {
                const size_t length = message->header.length;
                if ((length < sizeof(header_t)) || (length > sizeof(message_t)))
                    return STATUS_INVALID_VALUE;
                if (header_checksum(&message->header) != 0)
                    return STATUS_INVALID_VALUE;

                return buffer->write(message, length);
            }

        } /* namespace pipewire */
    } /* namespace audio */
} /* namespace lsp */


