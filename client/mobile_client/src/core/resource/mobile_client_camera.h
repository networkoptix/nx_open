#pragma once

#include <core/resource/client_core_camera.h>

class QnMobileClientCamera: public QnClientCoreCamera {
    Q_OBJECT

    typedef QnClientCoreCamera base_type;

public:
    QnMobileClientCamera(const QnUuid &resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider *dataProvider = 0) const override;

protected:
    virtual QnAbstractStreamDataProvider *createLiveDataProvider() override;
};
