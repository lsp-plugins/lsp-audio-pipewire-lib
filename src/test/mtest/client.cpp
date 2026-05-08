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

    struct client_t
    {
        test_type_t                *test;
        audio::pipewire::backend_t *back;
        audio::port_id_t            audio_in[2];
        audio::port_id_t            audio_out[2];
        size_t                      latency;
        size_t                      num_processed;
        bool                        connected;
        bool                        activated;
    };

    static status_t on_connected(void *user_data, const audio::io_parameters_t *params)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        test_type_t * const test = client->test;

        test->printf(
            "on_connected sample_rate=%d, buffer_size=%d, max_buffer_size=%d\n",
            int(params->sample_rate), int(params->buffer_size), int(params->max_buffer_size));

        MTEST_ASSERT_PTR(test, !client->connected);
        MTEST_ASSERT_PTR(test, !client->activated);

        client->connected       = true;

        // Register output ports
        audio::pipewire::backend_t * const back = client->back;

        client->audio_out[0]    = back->register_port(back, "out_l", audio::PORT_AUDIO_OUT);
        MTEST_ASSERT_PTR(test, client->audio_out[0] >= 0);
        client->audio_out[1]    = back->register_port(back, "out_r", audio::PORT_AUDIO_OUT);
        MTEST_ASSERT_PTR(test, client->audio_out[1] >= 0);

        audio::port_id_t dup_id = back->register_port(back, "out_l", audio::PORT_AUDIO_OUT);
        MTEST_ASSERT_PTR(test, dup_id == -STATUS_ALREADY_EXISTS);

        return STATUS_OK;
    }

    static status_t on_activated(void *user_data)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        test_type_t * const test = client->test;

        test->printf("on_activated\n");

        MTEST_ASSERT_PTR(test, !client->activated);
        MTEST_ASSERT_PTR(test, client->connected);

        client->activated       = true;

        return STATUS_OK;
    }

    static status_t on_io_changed(void *user_data, const audio::io_parameters_t *params)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        client->test->printf("on_io_changed\n");

        return STATUS_OK;
    }

    static status_t on_process(void *user_data, const audio::io_position_t *position, uint32_t frames)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        test_type_t * const test = client->test;
        audio::pipewire::backend_t * const back = client->back;

        test->printf("on_process\n");

        MTEST_ASSERT_PTR(test, client->activated);
        MTEST_ASSERT_PTR(test, client->connected);

        // Bypass audio signal
        for (size_t channel=0; channel<2; ++channel)
        {
            const audio::port_id_t out_id = client->audio_out[channel];

            // Get output buffer
            const size_t out_buffers = back->audio_buffers_count(back, out_id);
            if (out_buffers < 1)
                continue;
            float * const out = back->get_audio_buffer(back, out_id, 0);
            if (out == NULL)
            {
                test->printf("out == NULL\n");
                continue;
            }

            // Initialize output buffer
            for (size_t n=0; n<frames; ++n)
                out[n]          = 0.0f;

            // Get input buffers
            const audio::port_id_t in_id = client->audio_in[channel];
            const size_t in_buffers = back->audio_buffers_count(back, in_id);
            for (size_t j=0; j<in_buffers; ++j)
            {
                const float * const in = back->get_audio_buffer(back, in_id, j);
                if (in == NULL)
                    continue;

                // Mix input buffer contents to output buffer contents
                for (size_t n=0; n<frames; ++n)
                    out[n]         += in[n];
            }
        }

        // Change latency after 2 seconds of processing
        client->num_processed += frames;
        if ((client->num_processed >= 2*48000) && (client->latency == 0))
        {
            constexpr uint32_t new_latency = 512;
            status_t res = back->set_latency(back, new_latency);
            if (res == STATUS_OK)
                client->latency     = new_latency;
        }

        return STATUS_OK;
    }

    static status_t on_deactivated(void *user_data)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        test_type_t * const test = client->test;

        test->printf("on_deactivated\n");

        MTEST_ASSERT_PTR(test, client->activated);
        MTEST_ASSERT_PTR(test, client->connected);

        client->activated       = false;

        return STATUS_OK;
    }

    static void on_connection_lost(void *user_data)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        client->test->printf("on_connection_lost\n");

        client->connected       = false;
    }

    static void on_disconnected(void *user_data)
    {
        client_t * const client = static_cast<client_t *>(user_data);
        test_type_t * const test = client->test;
        client->test->printf("on_disconnected\n");

        MTEST_ASSERT_PTR(test, client->connected);
        MTEST_ASSERT_PTR(test, !client->activated);

        client->connected       = false;
    }

    MTEST_MAIN
    {
        using namespace audio;

        // Init client
        client_t client;
        client.test         = this;
        client.back         = NULL;
        client.audio_in[0]  = -1;
        client.audio_in[1]  = -1;
        client.audio_out[0] = -1;
        client.audio_out[1] = -1;
        client.latency      = 0;
        client.num_processed= 0;
        client.connected    = false;
        client.activated    = false;

        // Create backend
        printf("Creating backend...\n");
        pipewire::backend_t *back = static_cast<pipewire::backend_t *>(::malloc(sizeof(pipewire::backend_t)));
        MTEST_ASSERT(back!= NULL);
        lsp_finally {
            if (back)
                back->destroy(back);
        };
        back->construct();
        client.back         = back;

        // Register input ports
        client.audio_in[0]      = back->register_port(back, "in_l", audio::PORT_AUDIO_IN);
        MTEST_ASSERT(client.audio_in[0] >= 0);
        client.audio_in[1]      = back->register_port(back, "in_r", audio::PORT_AUDIO_IN);
        MTEST_ASSERT(client.audio_in[1] >= 0);

        MTEST_ASSERT(back->port_system_name(back, client.audio_in[0]) == NULL);
        MTEST_ASSERT(back->port_system_name(back, client.audio_in[1]) == NULL);

        audio::port_id_t dup_id = back->register_port(back, "in_l", audio::PORT_AUDIO_IN);
        MTEST_ASSERT(dup_id == -STATUS_ALREADY_EXISTS);

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

        MTEST_ASSERT(back->connect(back, &params, &backend_callbacks, &client) == STATUS_OK);

        // Ensure that connection call-back has been issued
        MTEST_ASSERT(client.connected == true);
        const char * port_name;

        port_name = back->port_system_name(back, client.audio_in[0]);
        MTEST_ASSERT((port_name != NULL) && (strcmp(port_name, "PipeWire Manual Test:in_l") == 0));
        port_name = back->port_system_name(back, client.audio_in[1]);
        MTEST_ASSERT((port_name != NULL) && (strcmp(port_name, "PipeWire Manual Test:in_r") == 0));
        port_name = back->port_system_name(back, client.audio_out[0]);
        MTEST_ASSERT((port_name != NULL) && (strcmp(port_name, "PipeWire Manual Test:out_l") == 0));
        port_name = back->port_system_name(back, client.audio_out[1]);
        MTEST_ASSERT((port_name != NULL) && (strcmp(port_name, "PipeWire Manual Test:out_r") == 0));

        sleep(60);

        // Disconnect backend
        printf("Disconnecting from PipeWire...\n");
        MTEST_ASSERT(back->disconnect(back) == STATUS_OK);

        MTEST_ASSERT(back->port_system_name(back, client.audio_in[0]) == NULL);
        MTEST_ASSERT(back->port_system_name(back, client.audio_in[1]) == NULL);
        MTEST_ASSERT(back->port_system_name(back, client.audio_out[0]) == NULL);
        MTEST_ASSERT(back->port_system_name(back, client.audio_out[1]) == NULL);

        // Ensure that disconnection call-back has been issued
        MTEST_ASSERT(client.connected == false);

        // Destroy backend
        printf("Destroying backend...\n");
        back->destroy(back);
        back = NULL;
    }

MTEST_END


