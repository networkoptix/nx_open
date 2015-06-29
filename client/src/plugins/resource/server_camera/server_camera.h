#ifndef QN_SERVER_CAMERA_H
#define QN_SERVER_CAMERA_H

#include <core/resource/camera_resource.h>
#include "camera/iomodule/iomodule_monitor.h"

class QnServerCamera;
typedef QnSharedResourcePointer<QnServerCamera> QnServerCameraPtr;
typedef QList<QnServerCameraPtr> QnServerCameraList;


class QnServerCamera: public QnVirtualCameraResource {
    Q_OBJECT

    typedef QnVirtualCameraResource base_type;
public:
    QnServerCamera(const QnUuid& resourceTypeId);

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override;

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider = 0) const override;

    virtual Qn::ResourceStatus getStatus() const override;

    virtual Qn::ResourceFlags flags() const override;
    virtual void setParentId(const QnUuid& parent) override;
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;
    
    QnIOModuleMonitorPtr openIOModuleMonitor();

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;

private:
    QnServerCameraPtr m_activeCamera;
};

#endif // QN_SERVER_CAMERA_H
