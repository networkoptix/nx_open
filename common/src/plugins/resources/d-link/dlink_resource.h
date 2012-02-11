#ifndef dlink_resource_h_2215
#define dlink_resource_h_2215

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"


struct QnDlink_cam_info
{
    QnDlink_cam_info():
    hasMPEG4(false),
    numberOfVideoProfiles(0),
    hasFixedQuality(false)
    {

    }

    bool inited() const
    {
        return numberOfVideoProfiles > 0;
    }

    // returns resolution with width not less than width
    QSize resolutionCloseTo(int width)
    {
        if (resolutions.size()==0)
        {
            Q_ASSERT(false);
            return QSize(0,0);
        }

        QSize result = resolutions.at(0);

        
        foreach(const QSize& size, resolutions)
        {
            if (size.width() <= width)
                return result;
            else
                result = size;
        }

        return result;
    }

    // returns next up bitrate 
    QString bitrateCloseTo(int val)
    {

        QSize result;

        if (possibleBitrates.size()==0)
        {
            Q_ASSERT(false);
            return "";
        }

        QMap<int, QString>::iterator it = possibleBitrates.lowerBound(val);
        if (it == possibleBitrates.end())
            it--;

        return it.value();

    }

    // returns next up frame rate 
    int frameRateCloseTo(int fr)
    {
        Q_ASSERT(possibleFps.size()>0);

        int result = possibleFps.at(0);

        foreach(int fps, possibleFps)
        {
            if (result <= fr)
                return result;
            else
                result = fps;
        }

        return result;
    }


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

    virtual int getMaxFps() override; 

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    QnDlink_cam_info getCamInfo() const;

    void init() override; // does a lot of physical work 

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping);
protected:
    
    
    QnDlink_cam_info  m_camInfo;
};

typedef QSharedPointer<QnPlDlinkResource> QnPlDlinkResourcePtr;

#endif //dlink_resource_h_2215
