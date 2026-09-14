// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
} // extern "C"

#include <algorithm>
#include <cstring>
#include <vector>

#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/video_data_packet.h>

namespace nx::media::ffmpeg::test {

namespace {

constexpr size_t kPayloadSize = 100;
constexpr uint8_t kPayloadFiller = 0x5a;
constexpr uint8_t kCanaryFiller = 0xab;

/** Video data with a buffer which does not reserve space for the ffmpeg padding. */
class UnpaddedVideoData: public QnCompressedVideoData
{
public:
    UnpaddedVideoData(size_t dataSize, size_t bufferSize):
        m_dataSize(dataSize),
        m_buffer(bufferSize, kCanaryFiller)
    {
        std::fill(m_buffer.begin(), m_buffer.begin() + dataSize, kPayloadFiller);
    }

    virtual QnCompressedVideoData* clone() const override { return nullptr; }
    virtual const char* data() const override { return (const char*) m_buffer.data(); }
    virtual size_t dataSize() const override { return m_dataSize; }
    virtual void setData(nx::utils::ByteArray&& /*buffer*/) override {}

    const std::vector<uint8_t>& buffer() const { return m_buffer; }

private:
    const size_t m_dataSize;
    std::vector<uint8_t> m_buffer;
};

} // namespace

TEST(AvPacket, paddedDataIsUsedInPlace)
{
    const std::vector<uint8_t> payload(kPayloadSize, kPayloadFiller);

    QnWritableCompressedVideoData data(kPayloadSize);
    data.m_data.uncheckedWrite((const char*) payload.data(), payload.size());
    data.timestamp = 42;
    data.flags |= QnAbstractMediaData::MediaFlags_AVKey;
    ASSERT_GE(data.paddingSize(), (size_t) AV_INPUT_BUFFER_PADDING_SIZE);

    AvPacket packet(&data);
    ASSERT_EQ(data.data(), (const char*) packet.get()->data);
    ASSERT_EQ((int) kPayloadSize, packet.get()->size);
    ASSERT_EQ(42, packet.get()->pts);
    ASSERT_EQ(AV_PKT_FLAG_KEY, packet.get()->flags & AV_PKT_FLAG_KEY);

    for (int i = 0; i < AV_INPUT_BUFFER_PADDING_SIZE; ++i)
        ASSERT_EQ(0, packet.get()->data[kPayloadSize + i]) << "Padding byte " << i;
}

TEST(AvPacket, unpaddedDataIsCopied)
{
    // The canary emulates the memory which belongs to somebody else, e.g. to the next frame in a
    // buffer owned by a camera plugin.
    UnpaddedVideoData data(kPayloadSize, kPayloadSize + AV_INPUT_BUFFER_PADDING_SIZE);
    data.timestamp = 42;
    ASSERT_EQ(0U, data.paddingSize());

    AvPacket packet(&data);
    ASSERT_NE(data.data(), (const char*) packet.get()->data);
    ASSERT_EQ((int) kPayloadSize, packet.get()->size);
    ASSERT_EQ(42, packet.get()->pts);
    ASSERT_EQ(0, memcmp(packet.get()->data, data.data(), kPayloadSize));

    for (size_t i = kPayloadSize; i < data.buffer().size(); ++i)
        ASSERT_EQ(kCanaryFiller, data.buffer()[i]) << "Overwritten byte " << i;

    for (int i = 0; i < AV_INPUT_BUFFER_PADDING_SIZE; ++i)
        ASSERT_EQ(0, packet.get()->data[kPayloadSize + i]) << "Padding byte " << i;
}

TEST(AvPacket, emptyData)
{
    UnpaddedVideoData data(/*dataSize*/ 0, /*bufferSize*/ 0);
    AvPacket packet(&data);
    ASSERT_EQ(0, packet.get()->size);
}

} // namespace nx::media::ffmpeg::test
