// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/sdp.h>
#include <nx/streaming/rtsp_client.h>

TEST(Sdp, SetupResponse1)
{
    using namespace nx::vms::api;
    using namespace std::chrono;

    QString value =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 25\r\n"
        "Session: SomeId13816;timeout=50\r\n"
        "Transport: RTP/AVP;multicast;destination=239.128.1.100;port=5564-5565;ttl=15;interleaved=2-3;ssrc=1234;\r\n"
        "\r\n";

    QnRtspClient rtspClient(QnRtspClient::Config{});
    QnRtspClient::SDPTrackInfo track;
    track.ioDevice.reset(new QnRtspIoDevice(&rtspClient, RtpTransportType::multicast));
    rtspClient.parseSetupResponse(value, &track, 0);

    ASSERT_EQ("239.128.1.100", track.sdpMedia.connectionAddress.toString());
    ASSERT_EQ(5564, track.ioDevice->mediaAddressInfo().address.port);
    ASSERT_EQ(5565, track.ioDevice->rtcpAddressInfo().address.port);

    ASSERT_EQ(2, track.interleaved.first);
    ASSERT_EQ(3, track.interleaved.second);

    ASSERT_EQ(0x1234, track.ioDevice->getSSRC());

    ASSERT_EQ("SomeId13816", rtspClient.sessionId());
    ASSERT_EQ(50s, rtspClient.keepAliveTimeOut());
}

TEST(Sdp, SetupResponse2)
{
    using namespace nx::vms::api;
    using namespace std::chrono;

    QString value =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 25\r\n"
        "Session: 13816;timeout=50\r\n"
        "Transport: RTP/AVP; multicast; estination=239.128.1.100; Server_Port=5564; ttl=15; Interleaved=2\r\n"
        "\r\n";

    QnRtspClient rtspClient(QnRtspClient::Config{});
    QnRtspClient::SDPTrackInfo track;
    track.ioDevice.reset(new QnRtspIoDevice(&rtspClient, RtpTransportType::multicast));
    rtspClient.parseSetupResponse(value, &track, 0);

    ASSERT_EQ(2, track.interleaved.first);
    ASSERT_EQ(3, track.interleaved.second);

    ASSERT_EQ(5564, track.ioDevice->mediaAddressInfo().address.port);
    ASSERT_EQ(5565, track.ioDevice->rtcpAddressInfo().address.port);
}
