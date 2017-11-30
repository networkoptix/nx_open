#ifndef isd_resource_h_1934
#define isd_resource_h_1934

#ifdef ENABLE_ISD

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include <nx/network/deprecated/simple_http_client.h>
#include "nx/streaming/media_data_packet.h"

class QnPlIsdResource : public QnPhysicalCameraResource
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

    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCroppingPhysical(QRect cropping);

private:
    void setMaxFps(int f);
    CameraDiagnostics::Result doISDApiRequest( const nx::utils::Url& apiRequestUrl, QByteArray* const msgBody );
};

#endif // #ifdef ENABLE_ISD

#endif //isd_resource_h_1934
