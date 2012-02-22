#ifndef dlink_resource_h_2215
#define dlink_resource_h_2215

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"


struct QnDlink_cam_info
{
    QnDlink_cam_info();

    void clear();

    bool inited() const;

    // returns resolution with width not less than width
    QSize resolutionCloseTo(int width);

    // returns next up bitrate 
    QString bitrateCloseTo(int val);

    // returns next up frame rate 
    int frameRateCloseTo(int fr);

    QString hasH264;// some cams have H.264, some H264
    bool hasMPEG4;
    bool hasFixedQuality;
    int numberOfVideoProfiles;
    QMap<int, QString> videoProfileUrls;
    QList<QSize> resolutions;

    QMap<int, QString> possibleBitrates;
    QList<int> possibleFps;
    QString possibleQualities;

};

class QnPlDlinkResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnPlDlinkResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    QnDlink_cam_info getCamInfo() const;

    void init() override; // does a lot of physical work 

    void setMotionMask(const QRegion& mask) override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping);

    void setMotionMaskPhysical();

protected:
    QnDlink_cam_info  m_camInfo;
};

typedef QnSharedResourcePointer<QnPlDlinkResource> QnPlDlinkResourcePtr;

#endif //dlink_resource_h_2215
