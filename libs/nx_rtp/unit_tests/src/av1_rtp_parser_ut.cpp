// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/codec/av1/av1_common.h>
#include <nx/rtp/parsers/av1_rtp_parser.h>

using namespace nx::media::av1;

namespace {

using Bytes = std::vector<uint8_t>;

nx::rtp::RtpHeader makeHeader(uint32_t timestamp, bool marker = false, uint16_t sequence = 0)
{
    nx::rtp::RtpHeader header{};
    header.version = nx::rtp::RtpHeader::kVersion;
    header.marker = marker;
    header.timestamp = htonl(timestamp);
    header.sequence = htons(sequence);
    return header;
}

Bytes leb128(uint64_t value)
{
    uint8_t buffer[kMaxLeb128Bytes];
    const int size = writeLeb128(value, buffer);
    return Bytes(buffer, buffer + size);
}

void append(Bytes* destination, const Bytes& source)
{
    destination->insert(destination->end(), source.begin(), source.end());
}

/**
 * @param obuElementCount The W field. 0 means every element gets a length field, otherwise the
 *     last element has no length field.
 */
Bytes makePacket(
    bool z, bool y, int obuElementCount, bool n, const std::vector<Bytes>& obuElements)
{
    uint8_t aggregationHeader = (uint8_t) (obuElementCount << 4);
    if (z)
        aggregationHeader |= 0x80;
    if (y)
        aggregationHeader |= 0x40;
    if (n)
        aggregationHeader |= 0x08;

    Bytes result{aggregationHeader};
    for (size_t i = 0; i < obuElements.size(); ++i)
    {
        const bool isLastDeclared = obuElementCount != 0 && i + 1 == (size_t) obuElementCount;
        if (!isLastDeclared)
            append(&result, leb128(obuElements[i].size()));
        append(&result, obuElements[i]);
    }
    return result;
}

/** An OBU as sent over RTP: no obu_size field. */
Bytes makeObu(ObuType type, const Bytes& payload)
{
    Bytes result{(uint8_t) (((int) type) << 3)};
    append(&result, payload);
    return result;
}

// clang-format off

// Sequence header OBU payload produced by libaom-av1 for 1920x1080.
const Bytes kSequenceHeaderPayload = {
    0x20, 0x00, 0x00, 0x42, 0xab, 0xbf, 0xc3, 0x73, 0x2b, 0xe4, 0x80, 0x86, 0x80, 0x10};

// uncompressed_header(): show_existing_frame = 0, frame_type = 0 (KEY_FRAME).
const Bytes kKeyFramePayload = {0x10, 0x22, 0x33, 0x44};

// uncompressed_header(): show_existing_frame = 0, frame_type = 1 (INTER_FRAME).
const Bytes kInterFramePayload = {0x30, 0x55, 0x66, 0x77};

// clang-format on

Bytes sequenceHeaderObu()
{
    return makeObu(ObuType::sequenceHeader, kSequenceHeaderPayload);
}
Bytes keyFrameObu()
{
    return makeObu(ObuType::frame, kKeyFramePayload);
}
Bytes interFrameObu()
{
    return makeObu(ObuType::frame, kInterFramePayload);
}

Bytes frameData(const QnAbstractMediaDataPtr& data)
{
    return Bytes((const uint8_t*) data->data(), (const uint8_t*) data->data() + data->dataSize());
}

/** Feeds one packet, asserting that parsing succeeded. */
bool process(nx::rtp::Av1Parser& parser, Bytes packet, const nx::rtp::RtpHeader& header)
{
    bool gotData = false;
    const auto result = parser.processData(
        header, packet.data(), /*bufferOffset*/ 0, (int) packet.size(), gotData);
    EXPECT_TRUE(result.success) << result.message.toStdString();
    return gotData;
}

/** Emits a complete single packet key frame temporal unit. */
QnAbstractMediaDataPtr feedKeyFrame(
    nx::rtp::Av1Parser& parser, uint32_t timestamp, uint16_t sequence)
{
    const auto packet = makePacket(false, false, 2, true, {sequenceHeaderObu(), keyFrameObu()});
    if (!process(parser, packet, makeHeader(timestamp, /*marker*/ true, sequence)))
        return nullptr;
    return parser.nextData();
}

} // namespace

TEST(Av1RtpParser, RejectsEmptyPayload)
{
    nx::rtp::Av1Parser parser;
    bool gotData = false;
    uint8_t payload = 0;

    const auto result = parser.processData(
        makeHeader(/*timestamp*/ 1), &payload, /*bufferOffset*/ 0, /*bytesRead*/ 0, gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(Av1RtpParser, RejectsPayloadWithoutObuElements)
{
    nx::rtp::Av1Parser parser;
    bool gotData = false;
    uint8_t payload = 0x10; //< Aggregation header only.

    const auto result = parser.processData(
        makeHeader(/*timestamp*/ 1), &payload, /*bufferOffset*/ 0, /*bytesRead*/ 1, gotData);

    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(Av1RtpParser, ProducesFrameOnMarkerBit)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 100, /*sequence*/ 1);
    ASSERT_TRUE(data);
    ASSERT_EQ(AV_CODEC_ID_AV1, data->compressionType);
    ASSERT_EQ(100u, (uint32_t) data->timestamp);
}

TEST(Av1RtpParser, OutputStartsWithTemporalDelimiter)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1);
    ASSERT_TRUE(data);

    const auto frame = frameData(data);
    ASSERT_GE(frame.size(), sizeof(kTemporalDelimiterObu));
    ASSERT_EQ(kTemporalDelimiterObu[0], frame[0]);
    ASSERT_EQ(kTemporalDelimiterObu[1], frame[1]);
}

TEST(Av1RtpParser, AddsObuSizeField)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1);
    ASSERT_TRUE(data);

    Bytes expected(kTemporalDelimiterObu, kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    // Both OBUs must be re-emitted with obu_has_size_field set and a leb128 obu_size.
    expected.push_back((((uint8_t) ObuType::sequenceHeader) << 3) | 0x02);
    append(&expected, leb128(kSequenceHeaderPayload.size()));
    append(&expected, kSequenceHeaderPayload);
    expected.push_back((((uint8_t) ObuType::frame) << 3) | 0x02);
    append(&expected, leb128(kKeyFramePayload.size()));
    append(&expected, kKeyFramePayload);

    ASSERT_EQ(expected, frameData(data));
}

TEST(Av1RtpParser, KeepsObuWithExistingSizeField)
{
    nx::rtp::Av1Parser parser;

    // A sender that leaves obu_size in place must not get a duplicated size field.
    Bytes sequenceHeader{(((uint8_t) ObuType::sequenceHeader) << 3) | 0x02};
    append(&sequenceHeader, leb128(kSequenceHeaderPayload.size()));
    append(&sequenceHeader, kSequenceHeaderPayload);

    const auto packet = makePacket(false, false, 2, true, {sequenceHeader, keyFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(1, /*marker*/ true, 1)));
    const auto data = parser.nextData();
    ASSERT_TRUE(data);

    Bytes expected(kTemporalDelimiterObu, kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    append(&expected, sequenceHeader); //< Unchanged: no second size field.
    expected.push_back((((uint8_t) ObuType::frame) << 3) | 0x02);
    append(&expected, leb128(kKeyFramePayload.size()));
    append(&expected, kKeyFramePayload);
    ASSERT_EQ(expected, frameData(data));
}

TEST(Av1RtpParser, RewritesObuWithExtensionHeader)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // obu_extension_flag set, temporal_id = 1, spatial_id = 0.
    Bytes metadata{(((uint8_t) ObuType::metadata) << 3) | 0x04, 0x20};
    const Bytes metadataPayload = {0xaa, 0xbb};
    append(&metadata, metadataPayload);

    const auto packet = makePacket(false, false, 2, false, {metadata, interFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(2, /*marker*/ true, 2)));
    const auto data = parser.nextData();
    ASSERT_TRUE(data);

    const auto frame = frameData(data);
    // Temporal delimiter, then the 2 byte OBU header, then obu_size, then the payload.
    ASSERT_EQ((((uint8_t) ObuType::metadata) << 3) | 0x04 | 0x02, frame[2]);
    ASSERT_EQ(0x20, frame[3]);
    ASSERT_EQ(metadataPayload.size(), frame[4]);
    ASSERT_EQ(metadataPayload[0], frame[5]);
    ASSERT_EQ(metadataPayload[1], frame[6]);
}

TEST(Av1RtpParser, AggregatesElementsWithoutElementCount)
{
    nx::rtp::Av1Parser parser;
    // W = 0: every element, including the last one, has a length field.
    const auto packet = makePacket(false, false, 0, true, {sequenceHeaderObu(), keyFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(1, /*marker*/ true, 1)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    ASSERT_TRUE(data->flags & QnAbstractMediaData::MediaFlags_AVKey);
}

TEST(Av1RtpParser, RejectsFewerElementsThanDeclared)
{
    nx::rtp::Av1Parser parser;
    // W = 3, but the payload ends after 2 length prefixed elements.
    Bytes packet{0x38}; //< W = 3, N = 1.
    append(&packet, leb128(sequenceHeaderObu().size()));
    append(&packet, sequenceHeaderObu());
    append(&packet, leb128(keyFrameObu().size()));
    append(&packet, keyFrameObu());

    bool gotData = false;
    const auto result =
        parser.processData(makeHeader(1), packet.data(), 0, (int) packet.size(), gotData);
    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(Av1RtpParser, RejectsElementLengthBeyondPayload)
{
    nx::rtp::Av1Parser parser;
    Bytes packet{0x08}; //< W = 0, N = 1.
    append(&packet, leb128(100));
    append(&packet, sequenceHeaderObu());

    bool gotData = false;
    const auto result =
        parser.processData(makeHeader(1), packet.data(), 0, (int) packet.size(), gotData);
    ASSERT_FALSE(result.success);
}

TEST(Av1RtpParser, RejectsTruncatedElementLength)
{
    nx::rtp::Av1Parser parser;
    const Bytes packet = {0x08, 0x80}; //< A leb128 value continuing past the end of the payload.

    bool gotData = false;
    Bytes buffer = packet;
    const auto result =
        parser.processData(makeHeader(1), buffer.data(), 0, (int) buffer.size(), gotData);
    ASSERT_FALSE(result.success);
}

TEST(Av1RtpParser, ReassemblesFragmentedObu)
{
    nx::rtp::Av1Parser parser;
    const auto sequenceHeader = sequenceHeaderObu();
    ASSERT_GT(sequenceHeader.size(), 6u);

    const Bytes part1(sequenceHeader.begin(), sequenceHeader.begin() + 3);
    const Bytes part2(sequenceHeader.begin() + 3, sequenceHeader.begin() + 6);
    const Bytes part3(sequenceHeader.begin() + 6, sequenceHeader.end());

    ASSERT_FALSE(
        process(parser, makePacket(false, true, 1, true, {part1}), makeHeader(1, false, 1)));
    ASSERT_FALSE(
        process(parser, makePacket(true, true, 1, false, {part2}), makeHeader(1, false, 2)));
    ASSERT_FALSE(
        process(parser, makePacket(true, false, 1, false, {part3}), makeHeader(1, false, 3)));
    ASSERT_TRUE(process(
        parser, makePacket(false, false, 1, false, {keyFrameObu()}), makeHeader(1, true, 4)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);

    Bytes expected(kTemporalDelimiterObu, kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    expected.push_back((((uint8_t) ObuType::sequenceHeader) << 3) | 0x02);
    append(&expected, leb128(kSequenceHeaderPayload.size()));
    append(&expected, kSequenceHeaderPayload);
    expected.push_back((((uint8_t) ObuType::frame) << 3) | 0x02);
    append(&expected, leb128(kKeyFramePayload.size()));
    append(&expected, kKeyFramePayload);
    ASSERT_EQ(expected, frameData(data));
}

TEST(Av1RtpParser, RejectsPendingFragmentAtMarkerBit)
{
    nx::rtp::Av1Parser parser;
    const auto sequenceHeader = sequenceHeaderObu();
    const Bytes part(sequenceHeader.begin(), sequenceHeader.begin() + 3);

    auto packet = makePacket(false, true, 1, true, {part});
    bool gotData = false;
    const auto result = parser.processData(
        makeHeader(1, /*marker*/ true, 1), packet.data(), 0, (int) packet.size(), gotData);
    ASSERT_FALSE(result.success);
    ASSERT_FALSE(gotData);
}

TEST(Av1RtpParser, DropsContinuationWithoutPendingFragment)
{
    nx::rtp::Av1Parser parser;
    // Z is set but nothing is pending: the start of the fragment was lost. This is not an error.
    const auto packet = makePacket(true, false, 1, false, {keyFrameObu()});
    ASSERT_FALSE(process(parser, packet, makeHeader(1, /*marker*/ true, 1)));
    ASSERT_FALSE(parser.nextData());
}

TEST(Av1RtpParser, DoesNotReportAnErrorForAnOrphanedFragmentTail)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // The packet that started the fragment has been lost, so the continuation holds the middle of
    // an OBU. Its first byte is not a valid OBU header, but that is packet loss, not corruption:
    // reporting an error here would count towards the RTSP error limit and reset the stream.
    const Bytes obuTail = {0xff, 0xfe, 0xfd};
    const auto packet = makePacket(true, false, 1, false, {obuTail});
    ASSERT_FALSE(process(parser, packet, makeHeader(2, /*marker*/ true, 2)));
    ASSERT_FALSE(parser.nextData());

    // The stream recovers on the next temporal unit.
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 3, /*sequence*/ 3));
}

TEST(Av1RtpParser, DoesNotEmitFrameUntilMarkerBit)
{
    nx::rtp::Av1Parser parser;
    ASSERT_FALSE(process(parser,
        makePacket(false, false, 1, true, {sequenceHeaderObu()}),
        makeHeader(1, false, 1)));
    ASSERT_FALSE(parser.nextData());
    ASSERT_TRUE(process(
        parser, makePacket(false, false, 1, false, {keyFrameObu()}), makeHeader(1, true, 2)));
    ASSERT_TRUE(parser.nextData());
}

TEST(Av1RtpParser, FlushesOnRtpTimestampChange)
{
    nx::rtp::Av1Parser parser;
    // No marker bit at all: the temporal unit must be flushed when the timestamp changes.
    ASSERT_FALSE(process(parser,
        makePacket(false, false, 2, true, {sequenceHeaderObu(), keyFrameObu()}),
        makeHeader(10, false, 1)));

    ASSERT_TRUE(process(
        parser, makePacket(false, false, 1, false, {interFrameObu()}), makeHeader(20, false, 2)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    ASSERT_EQ(10u, (uint32_t) data->timestamp); //< The flushed unit keeps its own timestamp.
}

TEST(Av1RtpParser, DropsInputTemporalDelimiter)
{
    nx::rtp::Av1Parser parser;
    const auto temporalDelimiter = makeObu(ObuType::temporalDelimiter, {});
    const auto packet =
        makePacket(false, false, 3, true, {temporalDelimiter, sequenceHeaderObu(), keyFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(1, /*marker*/ true, 1)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    const auto frame = frameData(data);
    // Exactly one temporal delimiter, the generated one.
    ASSERT_EQ(ObuType::temporalDelimiter, (ObuType) ((frame[0] >> 3) & 0x0f));
    ASSERT_EQ(ObuType::sequenceHeader, (ObuType) ((frame[2] >> 3) & 0x0f));
}

TEST(Av1RtpParser, DropsPaddingAndTileListObus)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    const auto packet = makePacket(false,
        false,
        3,
        false,
        {makeObu(ObuType::padding, {0x01, 0x02}),
            makeObu(ObuType::tileList, {0x03, 0x04}),
            interFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(2, /*marker*/ true, 2)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);

    Bytes expected(kTemporalDelimiterObu, kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    expected.push_back((((uint8_t) ObuType::frame) << 3) | 0x02);
    append(&expected, leb128(kInterFramePayload.size()));
    append(&expected, kInterFramePayload);
    ASSERT_EQ(expected, frameData(data));
}

TEST(Av1RtpParser, KeepsMetadataObu)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    const Bytes metadataPayload = {0x01, 0x02, 0x03};
    const auto packet = makePacket(
        false, false, 2, false, {makeObu(ObuType::metadata, metadataPayload), interFrameObu()});
    ASSERT_TRUE(process(parser, packet, makeHeader(2, /*marker*/ true, 2)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    ASSERT_EQ(ObuType::metadata, (ObuType) ((frameData(data)[2] >> 3) & 0x0f));
}

TEST(Av1RtpParser, SetsKeyFlagForSequenceHeaderAndKeyFrame)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1);
    ASSERT_TRUE(data);
    ASSERT_TRUE(data->flags & QnAbstractMediaData::MediaFlags_AVKey);
}

TEST(Av1RtpParser, DoesNotSetKeyFlagForInterFrame)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    ASSERT_TRUE(process(
        parser, makePacket(false, false, 1, false, {interFrameObu()}), makeHeader(2, true, 2)));
    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    ASSERT_FALSE(data->flags & QnAbstractMediaData::MediaFlags_AVKey);
}

TEST(Av1RtpParser, DoesNotSetKeyFlagForRepeatedSequenceHeaderWithInterFrame)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // Cameras often repeat the sequence header in every temporal unit.
    ASSERT_TRUE(process(parser,
        makePacket(false, false, 2, false, {sequenceHeaderObu(), interFrameObu()}),
        makeHeader(2, true, 2)));
    const auto data = parser.nextData();
    ASSERT_TRUE(data);
    ASSERT_FALSE(data->flags & QnAbstractMediaData::MediaFlags_AVKey);
}

TEST(Av1RtpParser, DropsTemporalUnitsBeforeFirstSequenceHeader)
{
    nx::rtp::Av1Parser parser;
    ASSERT_FALSE(process(
        parser, makePacket(false, false, 1, false, {interFrameObu()}), makeHeader(1, true, 1)));
    ASSERT_FALSE(parser.nextData());

    // The stream starts at the first sequence header.
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 2, /*sequence*/ 2));
}

TEST(Av1RtpParser, DropsTemporalUnitOnPacketLoss)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    ASSERT_FALSE(process(parser,
        makePacket(false, false, 1, false, {sequenceHeaderObu()}),
        makeHeader(2, false, 2)));
    // Sequence number 3 is lost.
    ASSERT_FALSE(process(
        parser, makePacket(false, false, 1, false, {keyFrameObu()}), makeHeader(2, true, 4)));
    ASSERT_FALSE(parser.nextData());

    // The next temporal unit is parsed normally.
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 3, /*sequence*/ 5));
}

TEST(Av1RtpParser, RecoversWhenTheMarkerPacketIsLost)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    ASSERT_FALSE(process(parser,
        makePacket(false, false, 1, false, {sequenceHeaderObu()}),
        makeHeader(2, false, 2)));
    // The rest of this temporal unit, including the packet with the marker bit, is lost.
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 3, /*sequence*/ 9));
}

TEST(Av1RtpParser, IgnoresDuplicatePacket)
{
    nx::rtp::Av1Parser parser;
    const auto packet = makePacket(false, false, 1, true, {sequenceHeaderObu()});
    ASSERT_FALSE(process(parser, packet, makeHeader(1, false, 1)));
    // The duplicate must be skipped entirely: appending its payload again would put a second copy
    // of every OBU it carries into the temporal unit.
    ASSERT_FALSE(process(parser, packet, makeHeader(1, false, 1)));
    ASSERT_TRUE(process(
        parser, makePacket(false, false, 1, false, {keyFrameObu()}), makeHeader(1, true, 2)));

    const auto data = parser.nextData();
    ASSERT_TRUE(data);

    Bytes expected(kTemporalDelimiterObu, kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    expected.push_back((((uint8_t) ObuType::sequenceHeader) << 3) | 0x02);
    append(&expected, leb128(kSequenceHeaderPayload.size()));
    append(&expected, kSequenceHeaderPayload);
    expected.push_back((((uint8_t) ObuType::frame) << 3) | 0x02);
    append(&expected, leb128(kKeyFramePayload.size()));
    append(&expected, kKeyFramePayload);
    ASSERT_EQ(expected, frameData(data));
}

TEST(Av1RtpParser, DropsTemporalUnitWhenItsFirstPacketIsLost)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // Sequence number 2, the first packet of the next temporal unit, is lost. It carried the frame
    // header, and here both packet boundaries are OBU boundaries, so the aggregation header gives
    // no clue: Z is not set and there is no pending fragment. The packets that did arrive must not
    // be emitted as if they were a complete frame.
    ASSERT_FALSE(process(parser,
        makePacket(false, false, 1, false, {makeObu(ObuType::tileGroup, {0x01, 0x02})}),
        makeHeader(2, /*marker*/ true, /*sequence*/ 3)));
    ASSERT_FALSE(parser.nextData());

    // The stream recovers on the next temporal unit.
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 3, /*sequence*/ 4));
}

TEST(Av1RtpParser, DoesNotEmitTemporalUnitWithoutAFrameHeader)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // Metadata, a sequence header and tile groups are not decodable without a frame header.
    ASSERT_FALSE(process(parser,
        makePacket(false,
            false,
            3,
            false,
            {makeObu(ObuType::metadata, {0x01}),
                sequenceHeaderObu(),
                makeObu(ObuType::tileGroup, {0x02})}),
        makeHeader(2, /*marker*/ true, /*sequence*/ 2)));
    ASSERT_FALSE(parser.nextData());
}

TEST(Av1RtpParser, EmitsTemporalUnitWithASeparateFrameHeader)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    // A frame may be sent as OBU_FRAME_HEADER followed by OBU_TILE_GROUP instead of one OBU_FRAME.
    ASSERT_TRUE(process(parser,
        makePacket(false,
            false,
            2,
            false,
            {makeObu(ObuType::frameHeader, kInterFramePayload),
                makeObu(ObuType::tileGroup, {0x01, 0x02})}),
        makeHeader(2, /*marker*/ true, /*sequence*/ 2)));
    ASSERT_TRUE(parser.nextData());
}

TEST(Av1RtpParser, IgnoresReservedAggregationHeaderBits)
{
    nx::rtp::Av1Parser parser;
    auto packet = makePacket(false, false, 2, true, {sequenceHeaderObu(), keyFrameObu()});
    packet[0] |= 0x07; //< The 3 reserved bits must not make the packet invalid.
    ASSERT_TRUE(process(parser, packet, makeHeader(1, /*marker*/ true, 1)));
    ASSERT_TRUE(parser.nextData());
}

TEST(Av1RtpParser, ExtractsResolutionFromSequenceHeader)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1);
    ASSERT_TRUE(data);

    const auto videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    ASSERT_TRUE(videoData);
    ASSERT_EQ(1920, videoData->width);
    ASSERT_EQ(1080, videoData->height);
}

TEST(Av1RtpParser, SetsCodecParametersOnKeyFrame)
{
    nx::rtp::Av1Parser parser;
    const auto data = feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1);
    ASSERT_TRUE(data);
    ASSERT_TRUE(data->context);

    const auto avCodecParameters = data->context->getAvCodecParameters();
    ASSERT_EQ(AV_CODEC_ID_AV1, avCodecParameters->codec_id);
    ASSERT_EQ(AVMEDIA_TYPE_VIDEO, avCodecParameters->codec_type);
    ASSERT_EQ(1920, avCodecParameters->width);
    ASSERT_EQ(1080, avCodecParameters->height);
}

TEST(Av1RtpParser, ClearWaitsForTheNextSequenceHeader)
{
    nx::rtp::Av1Parser parser;
    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 1, /*sequence*/ 1));

    parser.clear();

    // There are no out of band parameter sets for AV1, so nothing can be emitted until the next
    // sequence header arrives.
    ASSERT_FALSE(process(
        parser, makePacket(false, false, 1, false, {interFrameObu()}), makeHeader(2, true, 2)));
    ASSERT_FALSE(parser.nextData());

    ASSERT_TRUE(feedKeyFrame(parser, /*timestamp*/ 3, /*sequence*/ 3));
}

TEST(Av1RtpParser, SetsFrequencyFromSdp)
{
    nx::rtp::Av1Parser parser;
    ASSERT_EQ(90'000, parser.getFrequency());

    nx::rtp::Sdp::Media sdp;
    sdp.payloadType = 96;
    sdp.rtpmap.codecName = "AV1";
    sdp.rtpmap.clockRate = 90'000;
    // The fmtp parameters are ignored, but they must not upset setSdpInfo().
    sdp.fmtp.params = QStringList{"profile=0", "level-idx=5", "tier=0"};
    parser.setSdpInfo(sdp);

    ASSERT_EQ(90'000, parser.getFrequency());
}
