#include <gtest/gtest.h>

#include <nx/streaming/rtp/camera_time_helper.h>

using namespace nx::streaming::rtp;
using namespace std::chrono;

TEST(CameraTimeHelper, localOffset)
{
    CameraTimeHelper helper("test_resource", nullptr);
    helper.setResyncThreshold(milliseconds(1000));

    int frequency = 1000000;
    uint32_t rtpTime = 1000;
    microseconds currentTime(5000);
    microseconds time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(5000));

    currentTime += microseconds(1300);
    rtpTime += 1000;
    time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(6000));
}

TEST(CameraTimeHelper, rtpTimeOverflow)
{
    CameraTimeHelper helper("test_resource", nullptr);
    helper.setResyncThreshold(milliseconds(1000));
    int frequency = 1000000;

    uint32_t rtpTime = 0xfffffff0;
    microseconds currentTime(5000);
    microseconds time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(5000));

    currentTime += microseconds(20);
    rtpTime += 20;
    time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(5020));

    currentTime += microseconds(20);
    rtpTime -= 40;
    time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(4980));

    currentTime += microseconds(20);
    rtpTime += 20;
    time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, microseconds(5000));
}

TEST(CameraTimeHelper, resetTimeOffset)
{
    CameraTimeHelper helper("test_resource", nullptr);
    helper.setResyncThreshold(milliseconds(1000));
    int frequency = 1000000;

    uint32_t rtpTime = 1000;
    microseconds currentTime(seconds(5));
    microseconds time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, seconds(5));

    currentTime += seconds(1);
    rtpTime += 1000 * 1000 * 3; // rtpTime increase more faster than current time -> reset offset
    time = helper.getTime(
        currentTime, rtpTime, RtcpSenderReport(), std::nullopt, frequency, false);
    ASSERT_EQ(time, seconds(6));
}


