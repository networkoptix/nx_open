#ifndef __SERVER_CAMERA_H__
#define __SERVER_CAMERA_H__

#include "core/resourcemanagment/security_cam_resource.h"
#include "core/resource/network_resource.h"

class QnServerCameraProcessor : public QnResourceProcessor
{
public:
    void processResources(const QnResourceList &resources);
};

class QnServerCamera: public QnNetworkResource, public QnSequrityCamResource
{
public:
    QnServerCamera();

    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();
    virtual QString manufacture() const;
    virtual void setIframeDistance(int frames, int timems);

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) const override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) const override;
protected:
    virtual QString getUniqueId() const;
    virtual void setCropingPhysical(QRect croping);
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
};

class QnServerCameraFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);

    static QnServerCameraFactory& instance();
};

typedef QSharedPointer<QnServerCamera> QnServerCameraPtr;

#endif // __SERVER_CAMERA_H__
