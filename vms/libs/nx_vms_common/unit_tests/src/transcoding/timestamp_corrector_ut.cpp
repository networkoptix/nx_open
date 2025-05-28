// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <transcoding/timestamp_corrector.h>

using namespace std::chrono;
using namespace std::chrono_literals;

TEST(TimestampCorrector, singleStream)
{
    const seconds kMaxFrameDuration{5};
    const milliseconds kDefaultFrameDuration{1};
    TimestampCorrector m_corrector(kMaxFrameDuration, kDefaultFrameDuration);

    microseconds timestamp{5000};
    microseconds result{0};
    const milliseconds kRealFrameDuration{30};

    // Normal workflow.
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));
    timestamp += kRealFrameDuration;
    result += kRealFrameDuration;
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));
    timestamp += kRealFrameDuration;
    result += kRealFrameDuration;
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));

    // Gap to the future.
    timestamp += kMaxFrameDuration;
    result += kDefaultFrameDuration;
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));

    // Gap to the past.
    timestamp -= kMaxFrameDuration;
    result += kDefaultFrameDuration;
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));

    // Equal allowed.
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ true));

    // Equal.
    result += kDefaultFrameDuration;
    ASSERT_EQ(result, m_corrector.process(timestamp, /*stream*/ 0, /*Equal allowed*/ false));
}

TEST(TimestampCorrector, dualStream_keepGapsOnDependentStream)
{
    const seconds kMaxFrameDuration{1};
    const milliseconds kDefaultFrameDuration{30};
    TimestampCorrector m_corrector(kMaxFrameDuration, kDefaultFrameDuration);

    // Normal workflow.
    ASSERT_EQ(0ms    , m_corrector.process(1000ms, /*stream*/ 0, /*Equal allowed*/ false));
    ASSERT_EQ(100ms  , m_corrector.process(1100ms, /*stream*/ 1, /*Equal allowed*/ false));

    ASSERT_EQ(30ms   , m_corrector.process(1030ms, /*stream*/ 0, /*Equal allowed*/ false));
    ASSERT_EQ(130ms  , m_corrector.process(1130ms, /*stream*/ 1, /*Equal allowed*/ false));

    // Gap to the future by video -> should remove gap
    ASSERT_EQ(60ms   , m_corrector.process(3060ms, /*stream*/ 0, /*Equal allowed*/ false));
    ASSERT_EQ(160ms  , m_corrector.process(3160ms, /*stream*/ 1, /*Equal allowed*/ false));

    // Gap to the future by audio -> should keep gap
    ASSERT_EQ(90ms   , m_corrector.process(3090ms, /*stream*/ 0, /*Equal allowed*/ false));
    ASSERT_EQ(1160ms , m_corrector.process(4160ms, /*stream*/ 1, /*Equal allowed*/ false));
}
