#ifndef isd_resource_h_1934
#define isd_resource_h_1934

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QnPlIsdResource : public QnPhysicalCameraResource
{
public:

    static QString MAX_FPS_PARAM_NAME;

    static const char* MANUFACTURE;

    QnPlIsdResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual int getMaxFps() override;

    QSize getPrimaryResolution() const;
    QSize getSecondaryResolution() const;

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    //bool hasDualStreaming() const {return false;}


    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider) override;
protected:
    bool initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCropingPhysical(QRect croping);

private:
    void setMaxFps(int f);
    void save();

protected:
    QSize m_resolution1;
    QSize m_resolution2;

    

};

#endif //isd_resource_h_1934
