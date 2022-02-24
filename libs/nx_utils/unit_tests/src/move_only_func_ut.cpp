// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <future>
#include <optional>

#include <nx/utils/std/thread.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/random.h>

TEST(MoveOnlyFunc, common)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::utils::MoveOnlyFunc< void() > handler;
        std::optional< nx::utils::promise< bool > > promise;
        {
            promise = nx::utils::promise< bool >();
            handler = [&promise]() { promise->set_value(true); };
        }

        handler();
        ASSERT_TRUE(promise->get_future().get());
    }
}
