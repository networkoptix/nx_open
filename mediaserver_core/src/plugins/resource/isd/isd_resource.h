#pragma once

#ifdef ENABLE_ISD

#include <nx/mediaserver/resource/camera.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>

class QnPlIsdResource: public nx::mediaserver::resource::Camera
{
public:
    static QString MAX_FPS_PARAM_NAME;
    static const QString MANUFACTURE;

    QnPlIsdResource();

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    QSize getPrimaryResolution() const;
    QSize getSecondaryResolution() const;

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    //bool hasDualStreaming() const {return false;}

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

protected:
    QSize m_resolution1;
    QSize m_resolution2;

    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCroppingPhysical(QRect cropping);

private:
    void setMaxFps(int f);
    CameraDiagnostics::Result doISDApiRequest( const nx::utils::Url& apiRequestUrl, QByteArray* const msgBody );
};

#endif // #ifdef ENABLE_ISD
