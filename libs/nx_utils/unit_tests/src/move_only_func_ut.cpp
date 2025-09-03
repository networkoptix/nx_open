// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/move_only_func.h>
#include <nx/utils/random.h>

TEST(MoveOnlyFunc, common)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::MoveOnlyFunc< void() > handler;
        std::optional<std::promise<bool>> promise;
        {
            promise = std::promise<bool>();
            handler = [&promise]() { promise->set_value(true); };
        }

        handler();
        ASSERT_TRUE(promise->get_future().get());
    }
}
