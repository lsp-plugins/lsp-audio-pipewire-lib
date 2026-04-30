/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 30 апр. 2026 г.
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

#include <lsp-plug.in/audio/pipewire/backend.h>
#include <lsp-plug.in/common/finally.h>
#include <lsp-plug.in/test-fw/mtest.h>

MTEST_BEGIN("pipewire", client)

    static status_t on_connected(void *user_data, const audio::io_parameters_t *params)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_connected\n");

        return STATUS_OK;
    }

    static status_t on_activated(void *user_data)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_activated\n");

        return STATUS_OK;
    }

    static status_t on_io_changed(void *user_data, const audio::io_parameters_t *params)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_io_changed\n");

        return STATUS_OK;
    }

    static status_t on_process(void *user_data, const audio::io_position_t *position, uint32_t frames)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_process\n");

        return STATUS_OK;
    }

    static status_t on_deactivated(void *user_data)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_deactivated\n");

        return STATUS_OK;
    }

    static void on_connection_lost(void *user_data)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_connection_lost\n");
    }

    static void on_disconnected(void *user_data)
    {
        test_type_t * const self = static_cast<test_type_t *>(user_data);
        self->printf("on_disconnected\n");
    }

    MTEST_MAIN
    {
        using namespace audio;

        // Create backend
        printf("Creating backend...\n");
        pipewire::backend_t *back = static_cast<pipewire::backend_t *>(::malloc(sizeof(pipewire::backend_t)));
        MTEST_ASSERT(back!= NULL);
        lsp_finally {
            if (back)
                back->destroy(back);
        };
        back->construct();

        // Connect backend to PipeWire
        printf("Connecting to PipeWire...\n");

        static const audio::callbacks_t backend_callbacks = {
            .on_connected       = on_connected,
            .on_activated       = on_activated,
            .on_io_changed      = on_io_changed,
            .on_process         = on_process,
            .on_deactivated     = on_deactivated,
            .on_connection_lost = on_connection_lost,
            .on_disconnected    = on_disconnected,
        };

        connection_params_t params;
        params.client_name = "PipeWire Manual Test";
        params.url = "default";

        MTEST_ASSERT(back->connect(back, &params, &backend_callbacks, this) == STATUS_OK);

        // Disconnect backend
        printf("Disconnecting from PipeWire...\n");
        MTEST_ASSERT(back->disconnect(back) == STATUS_OK);

        // Destroy backend
        printf("Destroying backend...\n");
        back->destroy(back);
        back = NULL;
    }

MTEST_END


