#ifndef dlink_resource_h_2215
#define dlink_resource_h_2215

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"


struct QnDlink_cam_info
{
    QnDlink_cam_info():
    hasH264(false),
    hasMPEG4(false),
    numberOfVideoProfiles(0)
    {

    }

    bool inited() const
    {
        return numberOfVideoProfiles > 0;
    }

    bool hasH264;
    bool hasMPEG4;
    int numberOfVideoProfiles;
    QMap<int, QString> videoProfileUrls;
    QList<QSize> resolutions;

    QList<int> possibleFps;
    QString possibleQualities;

};

class QnPlDlinkResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlDlinkResource();

    virtual int getMaxFps() override; 

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    QnDlink_cam_info getCamInfo() const;

    void updateCamInfo(); // does a lot of physical work 

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping);


private Q_SLOTS:
    void onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus);

protected:
    
    
    QnDlink_cam_info  m_camInfo;
};

typedef QSharedPointer<QnPlDlinkResource> QnPlDlinkResourcePtr;

#endif //dlink_resource_h_2215
