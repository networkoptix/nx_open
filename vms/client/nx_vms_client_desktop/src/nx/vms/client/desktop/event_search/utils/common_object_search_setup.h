// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/utils/common_object_search_setup.h>

Q_MOC_INCLUDE("core/resource/camera_resource.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/text_filter_setup.h")

namespace nx::vms::client::desktop {

class SystemContext;
class WindowContext;

class CommonObjectSearchSetup: public core::CommonObjectSearchSetup
{
    Q_OBJECT
    using base_type = core::CommonObjectSearchSetup;

public:
    explicit CommonObjectSearchSetup(QObject* parent = nullptr);
    explicit CommonObjectSearchSetup(SystemContext* systemContext, QObject* parent = nullptr);

    virtual ~CommonObjectSearchSetup() override;

    WindowContext* context() const;

    void setContext(WindowContext* value);

    virtual bool selectCameras(QnUuidSet& selectedCameras) override;
    virtual QnVirtualCameraResourcePtr currentResource() const override;
    virtual QnVirtualCameraResourceSet currentLayoutCameras() const override;
    virtual void clearTimelineSelection() const override;

signals:
    void contextChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
