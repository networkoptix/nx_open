#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>
#include <utils/network/http/httpclient.h>
#include <core/dataprovider/stream_mixer.h>

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

    QnOpteraResource();
    virtual ~QnOpteraResource();

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

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
