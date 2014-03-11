#ifndef QN_SERVER_CAMERA_H
#define QN_SERVER_CAMERA_H

#include "core/resource/camera_resource.h"

class QnServerCamera;
typedef QnSharedResourcePointer<QnServerCamera> QnServerCameraPtr;

class QnLocalFileProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT
public:
    virtual void processResources(const QnResourceList &resources) override;
};

class QnServerCamera: public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnServerCamera(const QnId& resourceTypeId);

    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) override;

    QString getUniqueIdForServer(const QnResourcePtr mServer) const;

    QnServerCameraPtr findEnabledSibling();
    virtual Status getStatus() const override;

    void setTmpStatus(Status value);
protected:
    virtual QString getUniqueId() const override;
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;
private:
    Status m_tmpStatus;

private:
    QnServerCameraPtr m_activeCamera;
};

class QnServerCameraFactory : public QnResourceFactory
{
public:
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params) override;

    static QnServerCameraFactory& instance();
};

typedef QnSharedResourcePointer<QnServerCamera> QnServerCameraPtr;
typedef QList<QnServerCameraPtr> QnServerCameraList;

#endif // QN_SERVER_CAMERA_H
