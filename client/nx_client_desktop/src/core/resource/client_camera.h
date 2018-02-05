#pragma once

#include <core/resource/client_resource_fwd.h>
#include <core/resource/client_core_camera.h>

class QnArchiveStreamReader;

class QnClientCameraResource: public QnClientCoreCamera {
    Q_OBJECT

    typedef QnClientCoreCamera base_type;

public:
    QnClientCameraResource(const QnUuid &resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;

    virtual Qn::ResourceFlags flags() const override;

signals:
    void dataDropped(QnArchiveStreamReader* reader);

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;
};
