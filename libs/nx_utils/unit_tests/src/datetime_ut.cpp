// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cstdlib>

#include <gtest/gtest.h>

#include <nx/utils/datetime.h>
#include <nx/utils/time.h>

namespace nx::utils {
using namespace std::chrono;

TEST(DateTime, datetime_lock_free_validation)
{
    ASSERT_EQ(nx::utils::formatDateTime(
                  QDateTime::fromMSecsSinceEpoch(nx::utils::millisSinceEpoch().count())),
        nx::utils::formatDateTimeLockFree(nx::utils::utcTime().time_since_epoch()));
}

TEST(DateTime, datetime_parse_validation)
{
    auto reference = (nx::utils::millisSinceEpoch().count() / 1000) * 1000;
    auto referenceStr = nx::utils::formatDateTime(QDateTime::fromMSecsSinceEpoch(reference));
    auto actual = milliseconds(nx::utils::parseDateToMillisSinceEpoch(referenceStr));

    ASSERT_EQ(reference, actual.count());
}

} // namespace nx::utils
