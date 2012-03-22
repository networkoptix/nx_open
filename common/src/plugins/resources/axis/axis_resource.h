#ifndef axis_resource_h_2215
#define axis_resource_h_2215

#include <QSharedPointer>
#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlAxisResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlAxisResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual bool hasDualStreaming() const override { return false; }

    QString getMaxResolution() const;

protected:
    void init();
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
private:
    void clear();
private:
    QString m_maxResolution;
};

typedef QSharedPointer<QnPlAxisResource> QnPlAxisResourcePtr;

#endif //axis_resource_h_2215
