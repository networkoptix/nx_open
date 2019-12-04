#pragma once
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <transcoding/ffmpeg_transcoder.h>

namespace nx::vms::server::http_audio {

class AsyncChannelAudioConsumer: public QnAbstractDataConsumer
{
public:
    AsyncChannelAudioConsumer(nx::network::aio::AsyncChannelPtr socket);
    ~AsyncChannelAudioConsumer();
    virtual void putData(const QnAbstractDataPacketPtr& nonConstData) override;

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

private:
    bool initializeTranscoder(const QnAbstractDataPacketPtr& data);
    bool transcodeAndSend(const QnAbstractDataPacketPtr& data);

private:
    nx::network::aio::AsyncChannelPtr m_socket;
    std::unique_ptr<QnFfmpegTranscoder> m_transcoder;

};

} // namespace nx::vms::server::http_audio
