// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Adds event rules for intercom call and door opening.
 */
class NX_VMS_CLIENT_DESKTOP_API IntercomManager:
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit IntercomManager(
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~IntercomManager() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
