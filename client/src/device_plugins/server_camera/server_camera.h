#ifndef __SERVER_CAMERA_H__
#define __SERVER_CAMERA_H__

#include "core/resource/camera_resource.h"

class QnServerCameraProcessor : public QnResourceProcessor
{
public:
    void processResources(const QnResourceList &resources);
};

class QnServerCamera: public QnCameraResource
{
public:
    QnServerCamera();

    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();
    virtual QString manufacture() const;
    virtual void setIframeDistance(int frames, int timems);

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
protected:
    virtual QString getUniqueId() const;
    virtual void setCropingPhysical(QRect croping);
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
};

class QnServerCameraFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    static QnServerCameraFactory& instance();
};

typedef QSharedPointer<QnServerCamera> QnServerCameraPtr;

#endif // __SERVER_CAMERA_H__
