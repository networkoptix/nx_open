#ifndef QN_SERVER_CAMERA_H
#define QN_SERVER_CAMERA_H

#include <core/resource/camera_resource.h>

class QnServerCamera;
typedef QnSharedResourcePointer<QnServerCamera> QnServerCameraPtr;
typedef QList<QnServerCameraPtr> QnServerCameraList;


class QnServerCamera: public QnVirtualCameraResource {
    Q_OBJECT

public:
    QnServerCamera(const QnUuid& resourceTypeId);

    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;

    virtual Qn::ResourceStatus getStatus() const override;

    void setTmpStatus(Qn::ResourceStatus value);

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;

private:
    Qn::ResourceStatus m_tmpStatus;
    QnServerCameraPtr m_activeCamera;
};

#endif // QN_SERVER_CAMERA_H
