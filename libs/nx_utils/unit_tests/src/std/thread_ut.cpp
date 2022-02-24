// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/std/thread.h>

namespace nx::utils::test {

TEST(thread, very_fast_thread_function)
{
    nx::utils::thread t;
    ASSERT_NO_THROW(t = nx::utils::thread([]() {}));
    t.join();
}

} // namespace nx::utils::test
