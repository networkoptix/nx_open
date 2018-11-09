#include <gtest/gtest.h>

#include <nx/streaming/sdp.h>

TEST(Sdp, parsing)
{
    QString sdpString =
        "v=0\r\n"
        "s=PNO-9080R\r\n"
        "c=IN IP4 127.0.0.1\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=control:trackID=0\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1\r\n"
        "m=audio 0 RTP/AVP 14\r\n"
        "a=control:trackID=1\r\n"
        "a=rtpmap:14 MPA/90000\r\n\r\n";
    using namespace nx::streaming;
    Sdp sdp;
    sdp.parse(sdpString);
    ASSERT_EQ(sdp.media.size(), 2);
    ASSERT_EQ(sdp.media[0].format, 96);
    ASSERT_EQ(sdp.media[0].mediaType, Sdp::MediaType::Video);
    ASSERT_EQ(sdp.media[0].control, "trackID=0");
    ASSERT_EQ(sdp.media[0].sendOnly, false);
    ASSERT_EQ(sdp.media[0].rtpmap.codecName, "H264");
    ASSERT_EQ(sdp.media[0].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[0].rtpmap.channels, 0);

    ASSERT_EQ(sdp.media[1].format, 14);
    ASSERT_EQ(sdp.media[1].mediaType, Sdp::MediaType::Audio);
    ASSERT_EQ(sdp.media[1].control, "trackID=1");
    ASSERT_EQ(sdp.media[1].sendOnly, false);
    ASSERT_EQ(sdp.media[1].rtpmap.codecName, "MPA");
    ASSERT_EQ(sdp.media[1].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[1].rtpmap.channels, 0);


    QString sdpString1 = "v=0\r\n"
        "o=- 1533155478115363 1 IN IP4 192.168.0.123\r\n"
        "s=RTSP/RTP stream\r\n"
        "i=h264\r\n"
        "t=0 0\r\n"
        "a=tool:LIVE555 Streaming Media v2012.02.29\r\n"
        "a=type:broadcast\r\n"
        "a=control:*\r\n"
        "a=range:npt=0-\r\n"
        "a=x-qt-text-nam:RTSP/RTP stream\r\n"
        "a=x-qt-text-inf:h264\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "b=AS:4000\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=640028;sprop-parameter-sets=Z2QAKK2EBUViuKxUdCAqKxXFYqOhAVFYrisVHQgKisVxWKjoQFRWK4rFR0ICorFcVio6ECSFITk8nyfk/k/J8nm5s00IEkKQnJ5Pk/J/J+T5PNzZprQFAW0qQAAAAwCAAAAPGBAAD0JAAAiVQve+F4RCNQAAAAE=,aO48sA==\r\n"
        "a=control:track1\r\n"
        "m=application 0 RTP/AVP 98\r\n"
        "a=sendonly\r\n"
        "a=control:track2\r\n"
        "a=rtpmap:98 vnd.onvif.metadata/90000\r\n";
    sdp.parse(sdpString1);
    ASSERT_EQ(sdp.media.size(), 2);
    ASSERT_EQ(sdp.media[0].format, 96);
    ASSERT_EQ(sdp.media[0].mediaType, Sdp::MediaType::Video);
    ASSERT_EQ(sdp.media[0].control, "track1");
    ASSERT_EQ(sdp.media[0].sendOnly, false);
    ASSERT_EQ(sdp.media[0].rtpmap.codecName, "H264");
    ASSERT_EQ(sdp.media[0].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[0].rtpmap.channels, 0);

    ASSERT_EQ(sdp.media[1].format, 98);
    ASSERT_EQ(sdp.media[1].mediaType, Sdp::MediaType::Unknown);
    ASSERT_EQ(sdp.media[1].control, "track2");
    ASSERT_EQ(sdp.media[1].sendOnly, true);
    ASSERT_EQ(sdp.media[1].rtpmap.codecName, "vnd.onvif.metadata");
    ASSERT_EQ(sdp.media[1].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[1].rtpmap.channels, 0);
}


