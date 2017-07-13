#pragma once

#include <QtXml/QDomElement>

#include <nx/network/http/http_client.h>
#include <core/resource/resource_fwd.h>
#include <plugins/resource/onvif/dataprovider/rtp_stream_provider.h>
#include <plugins/resource/hikvision/hikvision_parsing_utils.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HikvisionHevcStreamReader: public QnRtpStreamReader
{

public:
    HikvisionHevcStreamReader(const QnResourcePtr& resource);

protected:
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;

private:
    QUrl buildHikvisionStreamUrl(int rtspPortNumber) const;
    QUrl hikvisionRequestUrlFromPath(const QString& path) const;

    QSize chooseResolution(
        const hikvision::ChannelCapabilities& channelCapabilities) const;

    QString chooseCodec(
        const hikvision::ChannelCapabilities& channelCapabilities) const;

    int chooseFps(
        const hikvision::ChannelCapabilities& channelCapabilities,
        float fps) const;

    boost::optional<int> chooseQuality(
        Qn::StreamQuality quality,
        const hikvision::ChannelCapabilities& channelCapabilities) const;

    boost::optional<int> chooseQuality(
        Qn::SecondStreamQuality quality,
        const hikvision::ChannelCapabilities& channelCapabilities) const;

    boost::optional<int> rescaleQuality(
        const std::vector<int>& outputQuality,
        int inputScaleSize,
        int inputQualityIndex) const;

    bool fetchChannelProperties(hikvision::ChannelProperties* outChannelProperties) const;

    CameraDiagnostics::Result configureChannel(
        QSize resolution,
        QString codec,
        int fps,
        boost::optional<int> quality);

    bool updateVideoChannelConfiguration(
        QDomDocument* outVideoChannelConfiguration,
        const QSize& resolution,
        const QString& codec,
        int fps,
        boost::optional<int> quality) const;

private:
    QnHikvisionResourcePtr m_hikvisionResource;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
