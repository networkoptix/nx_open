// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/timestamp_adjustment_history.h>

namespace nx::utils::test {

using namespace std::literals;

class TimestampAdjustmentHistoryTest: public ::testing::Test
{
public:
    TimestampAdjustmentHistory history{10s};

    TimestampAdjustmentHistoryTest()
    {
        history.record(100ms, 20ms);
        history.record(200ms, 30ms);
        history.record(240ms, 10ms);
        history.record(400ms, 80ms);
        history.record(410ms, -25ms);
        history.record(490ms, 34ms);
    }

    void assertIsOffsetBy(
        std::chrono::microseconds timestamp,
        std::chrono::microseconds expectedOffset)
    {
        ASSERT_EQ(history.replay(timestamp), timestamp + expectedOffset);
    }
};

TEST_F(TimestampAdjustmentHistoryTest, timestamps_between_records_are_affected_by_preceding_record)
{
    assertIsOffsetBy(150ms, 20ms);
    assertIsOffsetBy(230ms, 30ms);
    assertIsOffsetBy(409ms, 80ms);
    assertIsOffsetBy(410ms, -25ms);
}

TEST_F(TimestampAdjustmentHistoryTest, timestamps_exactly_on_records_are_affected_by_corresponding_records)
{
    assertIsOffsetBy(100ms, 20ms);
    assertIsOffsetBy(400ms, 80ms);
    assertIsOffsetBy(490ms, 34ms);
}

TEST_F(TimestampAdjustmentHistoryTest, timestamps_before_all_records_are_unaffected)
{
    assertIsOffsetBy(-99h, 0ms);
    assertIsOffsetBy(-1s, 0ms);
    assertIsOffsetBy(-100ms, 0ms);
    assertIsOffsetBy(0ms, 0ms);
    assertIsOffsetBy(99ms, 0ms);
}

TEST_F(TimestampAdjustmentHistoryTest, timestamps_after_all_records_are_affected_by_last_record)
{
    assertIsOffsetBy(491ms, 34ms);
    assertIsOffsetBy(500ms, 34ms);
    assertIsOffsetBy(10s, 34ms);
    assertIsOffsetBy(99h, 34ms);
}

TEST_F(TimestampAdjustmentHistoryTest, records_can_be_added_out_of_order)
{
    history.record(300ms, 15ms);

    assertIsOffsetBy(0ms, 0ms);
    assertIsOffsetBy(9ms, 0ms);
    assertIsOffsetBy(10ms, 0ms);
    assertIsOffsetBy(50ms, 0ms);
    assertIsOffsetBy(299ms, 10ms);
    assertIsOffsetBy(300ms, 15ms);
    assertIsOffsetBy(399ms, 15ms);
    assertIsOffsetBy(400ms, 80ms);

    history.record(10ms, 15ms);

    assertIsOffsetBy(0ms, 0ms);
    assertIsOffsetBy(9ms, 0ms);
    assertIsOffsetBy(10ms, 15ms);
    assertIsOffsetBy(50ms, 15ms);
    assertIsOffsetBy(103ms, 20ms);
    assertIsOffsetBy(299ms, 10ms);
    assertIsOffsetBy(300ms, 15ms);
    assertIsOffsetBy(350ms, 15ms);
    assertIsOffsetBy(399ms, 15ms);
    assertIsOffsetBy(400ms, 80ms);
}

TEST_F(TimestampAdjustmentHistoryTest, old_records_are_erased)
{
    history.record(10s + 300ms, 77ms);

    assertIsOffsetBy(150ms, 0ms);
    assertIsOffsetBy(210ms, 0ms);
    assertIsOffsetBy(239ms, 0ms);
    assertIsOffsetBy(240ms, 10ms);
    assertIsOffsetBy(405ms, 80ms);
    assertIsOffsetBy(5s, 34ms);
    assertIsOffsetBy(10s + 299ms, 34ms);
    assertIsOffsetBy(10s + 300ms, 77ms);
    assertIsOffsetBy(13s, 77ms);
}

} // namespace nx::utils::test
