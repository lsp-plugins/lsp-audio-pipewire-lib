/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-audio-pipewire-lib
 * Created on: 28 апр. 2026 г.
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

#include <lsp-plug.in/audio/pipewire/dictionary.h>
#include <lsp-plug.in/test-fw/utest.h>

UTEST_BEGIN("pipewire", dictionary)

    UTEST_TIMELIMIT(10)

    void test_dict()
    {
        using namespace audio::pipewire;

        printf("Testing basic dict functions...\n");
        printf("Creating dictionary...\n");

        dictionary dict;
        UTEST_ASSERT(dict.is_empty());

        // Create empty properties
        {
            pw_properties * const props = dict.make_properties();
            UTEST_ASSERT(props != NULL);
            pw_properties_free(props);
        }

        UTEST_ASSERT(dict.put("c", "1") == STATUS_OK);
        UTEST_ASSERT(dict.put("f", "2") == STATUS_OK);
        UTEST_ASSERT(dict.put("a", "3") == STATUS_OK);
        UTEST_ASSERT(dict.put("g", "4") == STATUS_OK);
        UTEST_ASSERT(dict.put("d", "5") == STATUS_OK);
        UTEST_ASSERT(dict.put("c", "6") == STATUS_OK);
        UTEST_ASSERT(dict.put("b", "7") == STATUS_OK);
        UTEST_ASSERT(dict.put("h", "8") == STATUS_OK);
        UTEST_ASSERT(dict.put("a", "9") == STATUS_OK);
        UTEST_ASSERT(dict.put("f", "10") == STATUS_OK);
        UTEST_ASSERT(dict.put("e", "11") == STATUS_OK);

        // Dict contents: abcdefgh
        UTEST_ASSERT(!dict.is_empty());
        UTEST_ASSERT(dict.size() == 8);

        // Check contains() call
        printf("Checking contains() method...\n");
        UTEST_ASSERT(dict.contains("a"));
        UTEST_ASSERT(dict.contains("b"));
        UTEST_ASSERT(dict.contains("c"));
        UTEST_ASSERT(dict.contains("d"));
        UTEST_ASSERT(dict.contains("e"));
        UTEST_ASSERT(dict.contains("f"));
        UTEST_ASSERT(dict.contains("g"));
        UTEST_ASSERT(dict.contains("h"));
        UTEST_ASSERT(!dict.contains("i"));
        UTEST_ASSERT(!dict.contains("z"));
        UTEST_ASSERT(!dict.contains("_"));

        // Keys should be sorted
        printf("Checking key() method...\n");
        UTEST_ASSERT((dict.key(0) != NULL) && (strcmp(dict.key(0), "a") == 0));
        UTEST_ASSERT((dict.key(1) != NULL) && (strcmp(dict.key(1), "b") == 0));
        UTEST_ASSERT((dict.key(2) != NULL) && (strcmp(dict.key(2), "c") == 0));
        UTEST_ASSERT((dict.key(3) != NULL) && (strcmp(dict.key(3), "d") == 0));
        UTEST_ASSERT((dict.key(4) != NULL) && (strcmp(dict.key(4), "e") == 0));
        UTEST_ASSERT((dict.key(5) != NULL) && (strcmp(dict.key(5), "f") == 0));
        UTEST_ASSERT((dict.key(6) != NULL) && (strcmp(dict.key(6), "g") == 0));
        UTEST_ASSERT((dict.key(7) != NULL) && (strcmp(dict.key(7), "h") == 0));
        UTEST_ASSERT(dict.key(8) == NULL);

        // Check values by index
        printf("Checking value(index) method...\n");
        UTEST_ASSERT((dict.value(size_t(0)) != NULL) && (strcmp(dict.value(size_t(0)), "9") == 0));
        UTEST_ASSERT((dict.value(1) != NULL) && (strcmp(dict.value(1), "7") == 0));
        UTEST_ASSERT((dict.value(2) != NULL) && (strcmp(dict.value(2), "6") == 0));
        UTEST_ASSERT((dict.value(3) != NULL) && (strcmp(dict.value(3), "5") == 0));
        UTEST_ASSERT((dict.value(4) != NULL) && (strcmp(dict.value(4), "11") == 0));
        UTEST_ASSERT((dict.value(5) != NULL) && (strcmp(dict.value(5), "10") == 0));
        UTEST_ASSERT((dict.value(6) != NULL) && (strcmp(dict.value(6), "4") == 0));
        UTEST_ASSERT((dict.value(7) != NULL) && (strcmp(dict.value(7), "8") == 0));
        UTEST_ASSERT(dict.value(8) == NULL);

        // Check values by key
        printf("Checking value(index) method...\n");
        UTEST_ASSERT((dict.value("a") != NULL) && (strcmp(dict.value("a"), "9") == 0));
        UTEST_ASSERT((dict.value("b") != NULL) && (strcmp(dict.value("b"), "7") == 0));
        UTEST_ASSERT((dict.value("c") != NULL) && (strcmp(dict.value("c"), "6") == 0));
        UTEST_ASSERT((dict.value("d") != NULL) && (strcmp(dict.value("d"), "5") == 0));
        UTEST_ASSERT((dict.value("e") != NULL) && (strcmp(dict.value("e"), "11") == 0));
        UTEST_ASSERT((dict.value("f") != NULL) && (strcmp(dict.value("f"), "10") == 0));
        UTEST_ASSERT((dict.value("g") != NULL) && (strcmp(dict.value("g"), "4") == 0));
        UTEST_ASSERT((dict.value("h") != NULL) && (strcmp(dict.value("h"), "8") == 0));
        UTEST_ASSERT((dict.value("i") == NULL) && (strcmp(dict.value("i", "X"), "X") == 0));

        printf("Checking destroy() method...\n");
        dict.destroy();
        UTEST_ASSERT(dict.is_empty());
        UTEST_ASSERT(dict.size() == 0);
    }

    void test_large_dict()
    {
        using namespace audio::pipewire;
        char key[32], value[32];

        printf("Testing large dictionary...\n");
        dictionary dict;
        UTEST_ASSERT(dict.is_empty());

        for (size_t i=0; i < 0x4000; ++i)
        {
            if ((i & 0xff) == 0)
                printf("  count == 0x%x...\n", int(i));
            snprintf(key, sizeof(key), "%08x%08x", int(rand()), int(rand()));
            snprintf(value, sizeof(value), "%08x%08x", int(rand()), int(rand()));

            UTEST_ASSERT(dict.put(key, value) == STATUS_OK);
        }
    }

    UTEST_MAIN
    {
        test_dict();
        test_large_dict();
    }

UTEST_END



