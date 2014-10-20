#ifndef isd_resource_h_1934
#define isd_resource_h_1934

#ifdef ENABLE_ISD

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QnPlIsdResource : public QnPhysicalCameraResource
{
public:

    static QString MAX_FPS_PARAM_NAME;

    static const QString MANUFACTURE;

    QnPlIsdResource();

    QSize getPrimaryResolution() const;
    QSize getSecondaryResolution() const;

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    //bool hasDualStreaming() const {return false;}

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCroppingPhysical(QRect cropping);

private:
    void setMaxFps(int f);
protected:
    QSize m_resolution1;
    QSize m_resolution2;

    

};

#endif // #ifdef ENABLE_ISD
#endif //isd_resource_h_1934
