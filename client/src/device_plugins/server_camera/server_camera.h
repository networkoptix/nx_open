#ifndef QN_SERVER_CAMERA_H
#define QN_SERVER_CAMERA_H

#include "core/resource/camera_resource.h"

class QnLocalFileProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT
public:
    void processResources(const QnResourceList &resources);
};

class QnServerCamera: public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnServerCamera();

    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems);

    virtual const QnResourceVideoLayout* getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;

    QString getUniqueIdForServer(const QnResourcePtr mServer) const;
protected:
    virtual QString getUniqueId() const override;
    virtual void setCropingPhysical(QRect croping);
    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
};

class QnServerCameraFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    static QnServerCameraFactory& instance();
};

typedef QnSharedResourcePointer<QnServerCamera> QnServerCameraPtr;
typedef QList<QnServerCameraPtr> QnServerCameraList;

#endif // QN_SERVER_CAMERA_H
