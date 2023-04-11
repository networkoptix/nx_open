// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/time/formatter.h>

namespace nx::vms::time::test {

using namespace std::chrono;

TEST(FormatterTest, fromNow)
{
    EXPECT_EQ("just now", fromNow(0s));
    EXPECT_EQ("just now", fromNow(59s));

    EXPECT_EQ("1 minute(s) ago", fromNow(60s));
    EXPECT_EQ("2 minute(s) ago", fromNow(125s));

    EXPECT_EQ("59 minute(s) ago", fromNow(59min));
    EXPECT_EQ("1 hour(s) ago", fromNow(60min));
    EXPECT_EQ("23 hour(s) ago", fromNow(23h + 10min));

    EXPECT_EQ("yesterday", fromNow(24h));
    EXPECT_EQ("yesterday", fromNow(47h));
    EXPECT_EQ("2 day(s) ago", fromNow(49h));
    EXPECT_EQ("6 day(s) ago", fromNow(24h * 6 + 10h));

    EXPECT_EQ("a week ago", fromNow(24h * 7 + 1h));

    EXPECT_EQ("10000000 day(s) ago", fromNow(24h * 10000000)); //< ~27397 years ago, incorrect time.

    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto date = QDateTime(QDate(1977, 5, 25), QTime(8, 59, 59));

    ASSERT_TRUE(now > date.toMSecsSinceEpoch());

    const milliseconds msecsAgo{now - date.toMSecsSinceEpoch()};

    EXPECT_EQ(toString(date, Format::d_MMMM_yyyy), fromNow(duration_cast<seconds>(msecsAgo)));
}

} // namespace nx::vms::time::test
