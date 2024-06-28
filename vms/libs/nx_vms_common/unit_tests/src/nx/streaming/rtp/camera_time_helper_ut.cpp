#include <gtest/gtest.h>

#include <nx/rtp/rtcp.h>
#include <nx/rtp/time_linearizer.h>
#include <nx/streaming/rtp/camera_time_helper.h>

using namespace nx::rtp;
using namespace nx::streaming::rtp;
using namespace std::chrono;

TEST(CameraTimeHelper, localOffset)
{
    CameraTimeHelper helper("test_resource", nullptr);
    helper.setResyncThreshold(milliseconds(1000));

    microseconds cameraTime = microseconds(1000);
    microseconds currentTime = microseconds(5000);
    microseconds time = helper.getTime(currentTime, cameraTime, false, false);
    ASSERT_EQ(time, microseconds(5000));

    currentTime += microseconds(1300);
    cameraTime += microseconds(1000);
    time = helper.getTime(currentTime, cameraTime, false, false);
    ASSERT_EQ(time, microseconds(6000));
}

TEST(CameraTimeHelper, rtpTimeOverflow)
{
    nx::rtp::TimeLinearizer<uint32_t> m_linearizer{0};
    uint32_t rtpTime = 0xfffffff0;
    int64_t reference = rtpTime;
    int64_t time = m_linearizer.linearize(rtpTime);
    ASSERT_EQ(time, rtpTime);

    rtpTime += 20;
    reference += 20;
    time = m_linearizer.linearize(rtpTime);
    ASSERT_EQ(time, reference);

    rtpTime -= 40;
    reference -= 40;
    time = m_linearizer.linearize(rtpTime);
    ASSERT_EQ(time, reference);

    rtpTime += 20;
    reference += 20;
    time = m_linearizer.linearize(rtpTime);
    ASSERT_EQ(time, reference);

    rtpTime += 20;
    reference += 20;
    time = m_linearizer.linearize(rtpTime);
    ASSERT_EQ(time, reference);
}

TEST(CameraTimeHelper, resetTimeOffset)
{
    CameraTimeHelper helper("test_resource", nullptr);
    helper.setResyncThreshold(milliseconds(1000));

    microseconds cameraTime = microseconds(1000);
    microseconds currentTime = seconds(5);
    microseconds time = helper.getTime(currentTime, cameraTime, false, false);
    ASSERT_EQ(time, seconds(5));

    currentTime += seconds(1);
    cameraTime += seconds(3); // rtpTime increase more faster than current time -> reset offset
    time = helper.getTime(currentTime, cameraTime, false, false);
    ASSERT_EQ(time, seconds(6));
}
