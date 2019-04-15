#include <gtest/gtest.h>

#include <nx/streaming/sdp.h>

using namespace nx::streaming;

TEST(Sdp, h264AndMpa)
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
    Sdp sdp;
    sdp.parse(sdpString);
    ASSERT_EQ(sdp.media.size(), 2);
    ASSERT_EQ(sdp.media[0].payloadType, 96);
    ASSERT_EQ(sdp.media[0].mediaType, Sdp::MediaType::Video);
    ASSERT_EQ(sdp.media[0].control, "trackID=0");
    ASSERT_EQ(sdp.media[0].sendOnly, false);
    ASSERT_EQ(sdp.media[0].rtpmap.codecName, "H264");
    ASSERT_EQ(sdp.media[0].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[0].rtpmap.channels, 0);

    ASSERT_EQ(sdp.media[1].payloadType, 14);
    ASSERT_EQ(sdp.media[1].mediaType, Sdp::MediaType::Audio);
    ASSERT_EQ(sdp.media[1].control, "trackID=1");
    ASSERT_EQ(sdp.media[1].sendOnly, false);
    ASSERT_EQ(sdp.media[1].rtpmap.codecName, "MPA");
    ASSERT_EQ(sdp.media[1].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[1].rtpmap.channels, 0);
}

TEST(Sdp, jpeg)
{
    QString sdpString =
        "v=0\r\n"
        "s=PNO-9080R\r\n"
        "c=IN IP4 127.0.0.1\r\n"
        "m=video 0 RTP/AVP 26\r\n"
        "a=control:trackID=0\r\n\r\n";

    Sdp sdp;
    sdp.parse(sdpString);
    ASSERT_EQ(sdp.media.size(), 1);
    ASSERT_EQ(sdp.media[0].payloadType, 26);
    ASSERT_EQ(sdp.media[0].mediaType, Sdp::MediaType::Video);
    ASSERT_EQ(sdp.media[0].control, "trackID=0");
    ASSERT_EQ(sdp.media[0].rtpmap.codecName, "JPEG");
}

TEST(Sdp, h264AndMetadata)
{
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

    Sdp sdp;
    sdp.parse(sdpString1);
    ASSERT_EQ("*", sdp.controlUrl);

    ASSERT_EQ(sdp.media.size(), 2);
    ASSERT_EQ(sdp.media[0].payloadType, 96);
    ASSERT_EQ(sdp.media[0].mediaType, Sdp::MediaType::Video);
    ASSERT_EQ(sdp.media[0].control, "track1");
    ASSERT_EQ(sdp.media[0].sendOnly, false);
    ASSERT_EQ(sdp.media[0].rtpmap.codecName, "H264");
    ASSERT_EQ(sdp.media[0].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[0].rtpmap.channels, 0);
    ASSERT_EQ(sdp.media[0].fmtp.params.size(), 3);
    ASSERT_EQ(sdp.media[0].fmtp.params[0], "packetization-mode=1");
    ASSERT_EQ(sdp.media[0].fmtp.params[1], "profile-level-id=640028");
    ASSERT_EQ(sdp.media[0].fmtp.params[2], "sprop-parameter-sets=Z2QAKK2EBUViuKxUdCAqKxXFYqOhAVFYrisVHQgKisVxWKjoQFRWK4rFR0ICorFcVio6ECSFITk8nyfk/k/J8nm5s00IEkKQnJ5Pk/J/J+T5PNzZprQFAW0qQAAAAwCAAAAPGBAAD0JAAAiVQve+F4RCNQAAAAE=,aO48sA==");

    ASSERT_EQ(sdp.media[1].payloadType, 98);
    ASSERT_EQ(sdp.media[1].mediaType, Sdp::MediaType::Unknown);
    ASSERT_EQ(sdp.media[1].control, "track2");
    ASSERT_EQ(sdp.media[1].sendOnly, true);
    ASSERT_EQ(sdp.media[1].rtpmap.codecName, "vnd.onvif.metadata");
    ASSERT_EQ(sdp.media[1].rtpmap.clockRate, 90000);
    ASSERT_EQ(sdp.media[1].rtpmap.channels, 0);
    ASSERT_EQ(sdp.media[1].fmtp.params.size(), 0);
}

TEST(Sdp, h264AndAudio)
{
    QString sdpString2 =
        "v=0\r\n"
        "o=- 1357696464263190 1 IN IP4 192.168.0.30\r\n"
        "s=Session streamed by \"ISD RTSP Server\"\r\n"
        "c=IN IP4 127.0.0.1\r\n"
        "t=0 0\r\n"
        "a=tool:ISD Streaming Media v0.1\r\n"
        "a=type:broadcast\r\n"
        "a=control:*\r\n"
        "a=range:npt=0-\r\n"
        "a=x-qt-text-nam:Session streamed by \"ISD RTSP Server\"\r\n"
        "m=video 4040 RTP/AVP 96\r\n"
        "b=AS:500\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1; profile-level-id=4D401E; sprop-parameter-sets=Z01AHppkBQF/y/+AEAANtwEBAUAAAPoAAA2sOhgdgB167y40MDsAOvXeXCg=,aO48gA==\r\n"
        "a=control:track1\r\n"
        "m=audio 0 RTP/AVP 97\r\n"
        "b=AS:12\r\n"
        "a=rtpmap:97 mpeg4-generic/8000/1\r\n"
        "a=fmtp:97 streamtype=5; profile-level-id=15; mode=AAC-hbr; sizeLength=13; indexLength=3; indexDeltaLength=3; profile=1; bitrate=12000; config=1588\r\n"
        "a=control:track2\r\n";

    Sdp sdp;
    sdp.parse(sdpString2);

    ASSERT_EQ(2, sdp.media.size());

    ASSERT_EQ("127.0.0.1", sdp.media[0].connectionAddress.toString());
    ASSERT_EQ("127.0.0.1", sdp.media[1].connectionAddress.toString());

    ASSERT_EQ(96, sdp.media[0].payloadType);
    ASSERT_EQ(Sdp::MediaType::Video, sdp.media[0].mediaType);
    ASSERT_EQ("track1", sdp.media[0].control);
    ASSERT_FALSE(sdp.media[0].sendOnly);
    ASSERT_EQ("H264", sdp.media[0].rtpmap.codecName);
    ASSERT_EQ(90000, sdp.media[0].rtpmap.clockRate);
    ASSERT_EQ(0, sdp.media[0].rtpmap.channels);
    ASSERT_EQ(3, sdp.media[0].fmtp.params.size());
    ASSERT_EQ("packetization-mode=1", sdp.media[0].fmtp.params[0]);
    ASSERT_EQ("profile-level-id=4D401E", sdp.media[0].fmtp.params[1]);
    ASSERT_EQ("sprop-parameter-sets=Z01AHppkBQF/y/+AEAANtwEBAUAAAPoAAA2sOhgdgB167y40MDsAOvXeXCg=,aO48gA==", sdp.media[0].fmtp.params[2]);
    ASSERT_EQ(4040, sdp.media[0].serverPort);

    ASSERT_EQ(97, sdp.media[1].payloadType);
    ASSERT_EQ(Sdp::MediaType::Audio, sdp.media[1].mediaType);
    ASSERT_EQ("track2", sdp.media[1].control);
    ASSERT_FALSE(sdp.media[1].sendOnly);
    ASSERT_EQ("mpeg4-generic", sdp.media[1].rtpmap.codecName);
    ASSERT_EQ(8000, sdp.media[1].rtpmap.clockRate);
    ASSERT_EQ(1, sdp.media[1].rtpmap.channels);
    ASSERT_EQ(9, sdp.media[1].fmtp.params.size());
    ASSERT_EQ("streamtype=5", sdp.media[1].fmtp.params[0]);
    ASSERT_EQ("profile-level-id=15", sdp.media[1].fmtp.params[1]);
    ASSERT_EQ("mode=AAC-hbr", sdp.media[1].fmtp.params[2]);
    ASSERT_EQ("sizeLength=13", sdp.media[1].fmtp.params[3]);
    ASSERT_EQ("indexLength=3", sdp.media[1].fmtp.params[4]);
    ASSERT_EQ("indexDeltaLength=3", sdp.media[1].fmtp.params[5]);
    ASSERT_EQ("profile=1", sdp.media[1].fmtp.params[6]);
    ASSERT_EQ("bitrate=12000", sdp.media[1].fmtp.params[7]);
    ASSERT_EQ("config=1588", sdp.media[1].fmtp.params[8]);
}


TEST(Sdp, ConnectionAddress)
{
    QString sdpString =
        "v=0\r\n"
        "o=- 1001 1 IN IP4 192.168.0.86\r\n"
        "s=VCP IPC Realtime stream\r\n"
        "m=video 5002 RTP/AVP 105\r\n"
        "c=IN IP4 241.0.0.2/127\r\n"
        "a=control:rtsp://192.168.0.86/media/video2/multicast/video\r\n"
        "a=rtpmap:105 H264/90000\r\n"
        "a=fmtp:105 profile-level-id=64001e; packetization-mode=1; sprop-parameter-sets=Z2QAHqwsaoLQSabgICAoAAAfQAAAfQQg,aO4xshs=\r\n"
        "a=recvonly\r\n"
        "m=audio 5004 RTP/AVP 0\r\n"
        "c=IN IP4 241.0.0.2/127\r\n"
        "a=fmtp:0 RTCP=0\r\n"
        "a=control:rtsp://192.168.0.86/media/video0/multicast/audio\r\n"
        "a=recvonly\r\n"
        "m=application 0 RTP/AVP 107\r\n"
        "c=IN IP4 241.0.0.2/127\r\n"
        "a=control:rtsp://192.168.0.86/media/video2/multicast/metadata\r\n"
        "a=rtpmap:107 vnd.onvif.metadata/90000\r\n"
        "a=fmtp:107 DecoderTag=h3c-v3 RTCP=0\r\n"
        "a=recvonly\r\n";
    Sdp sdp;
    sdp.parse(sdpString);

    ASSERT_EQ(3, sdp.media.size());

    ASSERT_EQ("241.0.0.2", sdp.media[0].connectionAddress.toString());
    ASSERT_EQ("241.0.0.2", sdp.media[1].connectionAddress.toString());
    ASSERT_EQ("241.0.0.2", sdp.media[2].connectionAddress.toString());
}
