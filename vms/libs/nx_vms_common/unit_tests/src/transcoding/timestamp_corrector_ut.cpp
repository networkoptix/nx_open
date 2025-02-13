// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <transcoding/timestamp_corrector.h>

TEST(TimestampCorrector, singleStream)
{
    const std::chrono::seconds kMaxFrameDuration{5};
    const std::chrono::milliseconds kDefaultFrameDuration{1};
    TimestampCorrector m_corrector(kMaxFrameDuration, kDefaultFrameDuration);

    std::chrono::microseconds timestamp{5000};
    std::chrono::microseconds result{0};
    const std::chrono::milliseconds kRealFrameDuration{30};

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

TEST(TimestampCorrector, dualStream)
{
    const std::chrono::seconds kMaxFrameDuration{5};
    const std::chrono::milliseconds kDefaultFrameDuration{1};
    TimestampCorrector m_corrector(kMaxFrameDuration, kDefaultFrameDuration);

    std::chrono::microseconds prevResult1{0};
    std::chrono::microseconds prevResult2{0};
    auto process = [&](std::chrono::microseconds timestamp, int stream)
    {
        auto result = m_corrector.process(timestamp, /*stream*/ stream, /*Equal allowed*/ false);
        auto& prevResult = stream == 1 ? prevResult1 : prevResult2;
        if (prevResult != std::chrono::microseconds::zero()) //< Not a first step.
        {
            ASSERT_GT(result, prevResult);
            ASSERT_LT(result - prevResult, kMaxFrameDuration);
        }
        prevResult = result;
        NX_DEBUG(NX_SCOPE_TAG, "result: %1, stream: %2", result, stream);
    };

    const std::chrono::milliseconds kRealFrameDuration{30};
    // Normal workflow 1 stream.
    std::chrono::milliseconds timestamp1{5000};
    process(timestamp1, 1);

    timestamp1 += kRealFrameDuration;
    process(timestamp1, 1);

    // Normal workflow 2 stream.
    std::chrono::milliseconds timestamp2{4500};
    process(timestamp2, 2);

    timestamp2 += kRealFrameDuration;
    process(timestamp2, 2);

    // Gap to the future by second stream.
    timestamp2 += kMaxFrameDuration;
    timestamp1 += kMaxFrameDuration;
    process(timestamp2, 2);

    // Normal workflow 1 stream.
    timestamp1 += kRealFrameDuration;
    process(timestamp1, 1);

    // Normal workflow 2 stream.
    timestamp2 += kRealFrameDuration;
    process(timestamp2, 2);

    // Gap to the past by first stream.
    timestamp1 -= kMaxFrameDuration;
    timestamp2 -= kMaxFrameDuration;
    process(timestamp1, 1);

    // Normal workflow 1 stream.
    timestamp1 += kRealFrameDuration;
    process(timestamp1, 1);

    // Normal workflow 2 stream.
    timestamp2 += kRealFrameDuration;
    process(timestamp2, 2);
}
