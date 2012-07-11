#ifndef QN_SERVER_CAMERA_H
#define QN_SERVER_CAMERA_H

#include "core/resource/camera_resource.h"

class QnServerCameraProcessor : public QObject, public QnResourceProcessor
{
    Q_OBJECT
public:
    void processResources(const QnResourceList &resources);
private slots:
    void at_serverIfFound(QString);
};

class QnServerCamera: public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnServerCamera();

    virtual bool isResourceAccessible();
    virtual bool updateMACAddress();
    virtual QString manufacture() const;
    virtual void setIframeDistance(int frames, int timems);

    virtual const QnVideoResourceLayout* getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider = 0) override;
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
