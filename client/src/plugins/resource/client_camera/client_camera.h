#pragma once

#include <plugins/resource/client_core_camera/client_core_camera.h>

class QnClientCamera: public QnClientCoreCamera {
    Q_OBJECT

    typedef QnClientCoreCamera base_type;

public:
    QnClientCamera(const QnUuid &resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;

    virtual Qn::ResourceFlags flags() const override;
    
protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;
};
