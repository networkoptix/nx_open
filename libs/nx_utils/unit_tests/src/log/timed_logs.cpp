// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log_main.h>

namespace nx::test {

class TimedLogs: public ::testing::Test
{
public:
    void logMs(size_t total) const
    {
        for (size_t i = 0; i < total; ++i)
        {
            NX_VERBOSE(this, "Millisecond %1", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void logS(size_t total) const
    {
        for (size_t i = 0; i < total; ++i)
        {
            NX_DEBUG(this, "Second %1", i);
            logMs(1000);
        }
    }
};

TEST_F(TimedLogs, DISABLED_s1) { logS(1); }
TEST_F(TimedLogs, DISABLED_s10) { logS(10); }
TEST_F(TimedLogs, DISABLED_s50) { logS(50); }

} // namespace nx::test
