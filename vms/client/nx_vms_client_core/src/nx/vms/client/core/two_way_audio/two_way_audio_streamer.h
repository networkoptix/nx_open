// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <memory>

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/media/audio_data_packet.h>
#include <nx/network/aio/async_channel_adapter.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/lockable.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <transcoding/ffmpeg_muxer.h>

namespace nx::vms::client::core {

class TwoWayAudioStreamer:
    public QnAbstractMediaDataReceptor,
    public std::enable_shared_from_this<TwoWayAudioStreamer>
{
public:
    TwoWayAudioStreamer(std::unique_ptr<QnAbstractStreamDataProvider> provider);
    virtual ~TwoWayAudioStreamer();
    bool start(
        const nx::network::http::Credentials& credentials,
        const QnVirtualCameraResourcePtr& camera);

    void stop();

public: // QnAbstractMediaDataReceptor impl.
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual void clearUnprocessedData() override;
    virtual bool needConfigureProvider() const { return false; } //< for DesktopDataProvider this means - No Video

private:
    void onUpgradeHttpClient();
    void onHttpRequest();
    void onDataSent();
    void startProvider();

private:
    bool initializeMuxer(const QnCompressedAudioDataPtr& data);
    void pushPacket(nx::utils::ByteArray buffer);
    int bufferSize();

private:
    const bool m_useWebsocket = false;
    nx::Buffer m_dataBuffer;
    std::unique_ptr<FfmpegMuxer> m_muxer;
    nx::Lockable<std::deque<std::unique_ptr<nx::utils::ByteArray>>> m_bufferQueue;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    nx::network::aio::AsyncChannelPtr m_streamChannel;
    std::unique_ptr<QnAbstractStreamDataProvider> m_provider;
    std::atomic<bool> m_sendInProcess = false;
};

} // namespace nx::vms::client::core
