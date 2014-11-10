#ifndef dlink_resource_h_2215
#define dlink_resource_h_2215

#ifdef ENABLE_DLINK

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"


struct QnDlink_cam_info
{
    QnDlink_cam_info();

    void clear();

    bool inited() const;

    // returns resolution with width not less than width
    QSize resolutionCloseTo(int width) const;

    // returns next up bitrate 
    QString bitrateCloseTo(int val);

    // returns next up frame rate 
    int frameRateCloseTo(int fr);

    QSize primaryStreamResolution() const;
    QSize secondaryStreamResolution() const;

    QString hasH264;// some cams have H.264, some H264
    bool hasMPEG4;
    bool hasFixedQuality;
    int numberOfVideoProfiles;
    QMap<int, QString> videoProfileUrls;
    QList<QSize> resolutions;

    QMap<int, QString> possibleBitrates;
    QList<int> possibleFps;
    QStringList possibleQualities;

};

class QnPlDlinkResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnPlDlinkResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    QnDlink_cam_info getCamInfo() const;

    
    virtual void setMotionMaskPhysical(int channel) override;

protected:
    virtual CameraDiagnostics::Result initInternal() override; // does a lot of physical work 
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCroppingPhysical(QRect cropping);

    

protected:
    QnDlink_cam_info  m_camInfo;
};

typedef QnSharedResourcePointer<QnPlDlinkResource> QnPlDlinkResourcePtr;

#endif // ENABLE_DLINK
#endif //dlink_resource_h_2215
