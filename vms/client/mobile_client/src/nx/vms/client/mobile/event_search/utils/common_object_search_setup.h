// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/utils/common_object_search_setup.h>

namespace nx::vms::client::mobile {

class CommonObjectSearchSetup: public core::CommonObjectSearchSetup
{
    Q_OBJECT
    using base_type = core::CommonObjectSearchSetup;

public:
    static void registerQmlType();

    explicit CommonObjectSearchSetup(
        core::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~CommonObjectSearchSetup() override;

    virtual bool selectCameras(UuidSet& selectedCameras) override;
    virtual QnVirtualCameraResourcePtr currentResource() const override;
    virtual QnVirtualCameraResourceSet currentLayoutCameras() const override;
    virtual void clearTimelineSelection() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
