#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>
#include <nx/network/http/http_client.h>
#include <providers/stream_mixer.h>

class QnOpteraResource : public QnPlOnvifResource
{
    typedef QnPlOnvifResource base_type;

    struct OpteraChannelInfo
    {
        QString path;
        QPoint channelPosition;
        quint32 channelNumber;
    };

public:

    QnOpteraResource(QnMediaServerModule* serverModule);
    virtual ~QnOpteraResource();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

private:
    CLHttpStatus makeGetStitchingModeRequest(CLSimpleHTTPClient& client, QByteArray& response) const;
    CLHttpStatus makeSetStitchingModeRequest(CLSimpleHTTPClient& http, const QString& mode) const;

    QString getCurrentStitchingMode(const QByteArray& page) const;

    bool initSubChannels();

private:
    std::map<int, QnPlOnvifResourcePtr> m_channelResources;
    mutable QnResourceVideoLayoutPtr m_videoLayout;
};

#endif // #ifdef ENABLE_ONVIF
