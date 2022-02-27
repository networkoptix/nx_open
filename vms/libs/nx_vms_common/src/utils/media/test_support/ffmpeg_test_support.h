// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/media/sdk_support/ffmpeg_sdk_support.h>
#include <nx/utils/log/assert.h>
#include <camera/camera_plugin.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

namespace nx::utils::media::test_support {

static const int kdrawPixelSize = 16;

NX_VMS_COMMON_API uint64_t getTimestampFromFrame(const AVFrame* frame);

class NX_VMS_COMMON_API AVPacketWithTimestampGenerator
{
public:
    AVPacketWithTimestampGenerator(
        AVCodecID codecId, int width, int height);
    ~AVPacketWithTimestampGenerator();

    AVCodecContext* getAvCodecContext() { return m_codecContext; };

    AVPacket* next(AVRational streamTimeBase);

private:
    AVCodecContext* m_codecContext = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
};

class NX_VMS_COMMON_API TimeStampedDataProvider: public QnAbstractMediaStreamDataProvider
{
public:
    TimeStampedDataProvider(const QnResourcePtr& resource, int maxPacketCount);
    ~TimeStampedDataProvider();

    void setAction(std::function<void()> action, int packetNumber);

private:
    AVPacketWithTimestampGenerator m_generator;

    static constexpr int kWidth = 640;
    static constexpr int kHeight = 640;
    static constexpr AVCodecID kCodecId = AV_CODEC_ID_H264;
    int m_packetCount = 0;
    int m_maxPacketCount = -1;
    std::function<void()> m_action;
    int m_actionPacketNumber = -1;

    virtual void run() override;
};

} // namespace nx::utils::media::test_support
