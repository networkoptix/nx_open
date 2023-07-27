// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "access_controller.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API CachingAccessController: public AccessController
{
    Q_OBJECT
    using base_type = AccessController;

public:
    explicit CachingAccessController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~CachingAccessController() override;

signals:
    void permissionsChanged(const QnResourcePtr& resource, Qn::Permissions old,
        Qn::Permissions current, QPrivateSignal);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
