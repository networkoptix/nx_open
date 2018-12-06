#pragma once

#if defined(ENABLE_ONVIF)

#include <QtXml/QDomElement>

#include <nx/network/http/http_client.h>
#include <core/resource/resource_fwd.h>
#include <streaming/rtp_stream_reader.h>
#include <plugins/resource/hikvision/hikvision_utils.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HikvisionHevcStreamReader: public QnRtpStreamReader
{

public:
    HikvisionHevcStreamReader(const HikvisionResourcePtr& resource);

protected:
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

private:
    nx::utils::Url buildHikvisionStreamUrl(
        const hikvision::ChannelProperties& properties) const;
    nx::utils::Url hikvisionRequestUrlFromPath(const QString& path) const;

    QSize chooseResolution(
        const hikvision::ChannelCapabilities& channelCapabilities,
        const QSize& primaryResolution) const;

    QString chooseCodec(
        const hikvision::ChannelCapabilities& channelCapabilities,
        AVCodecID codec) const;

    int chooseFps(
        const hikvision::ChannelCapabilities& channelCapabilities,
        float fps) const;

    boost::optional<int> chooseQuality(
        Qn::StreamQuality quality,
        const hikvision::ChannelCapabilities& channelCapabilities) const;

    boost::optional<int> rescaleQuality(
        const std::vector<int>& outputQuality,
        int inputScaleSize,
        int inputQualityIndex) const;

    CameraDiagnostics::Result fetchChannelProperties(
        hikvision::ChannelProperties* outChannelProperties) const;

    CameraDiagnostics::Result fetchRtspPortViaIsapi(boost::optional<int>* outRtspPort) const;

    CameraDiagnostics::Result fetchRtspPortViaOldApi(boost::optional<int>* rtspPort) const;

    CameraDiagnostics::Result configureChannel(
        const hikvision::ChannelProperties& channelProperties,
        QSize resolution,
        QString codec,
        int fps,
        boost::optional<int> quality,
        boost::optional<int> bitrateKbps);

    bool updateVideoChannelConfiguration(
        QDomDocument* outVideoChannelConfiguration,
        const QSize& resolution,
        const QString& codec,
        int fps,
        boost::optional<int> quality,
        boost::optional<int> bitrateKbps) const;

    CameraDiagnostics::Result fetchResponse(const nx::utils::Url& url, nx::Buffer* outBuffer) const;

private:
    HikvisionResourcePtr m_hikvisionResource;
    QString m_previousCodecValue;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif // ENABLE_ONVIF
