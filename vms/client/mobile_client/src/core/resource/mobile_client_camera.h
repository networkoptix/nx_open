#pragma once

#include <core/resource/client_core_camera.h>

class QnMobileClientCamera: public nx::vms::client::core::Camera
{
    Q_OBJECT
    using base_type = nx::vms::client::core::Camera;

public:
    explicit QnMobileClientCamera(const QnUuid& resourceTypeId);

    virtual QnConstResourceVideoLayoutPtr getVideoLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider = nullptr) const override;

protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};
