// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(CloudService,
    none = 0,
    cloud_notifications = 1 << 0,
    push_notifications = 1 << 1,
    docdb = 1 << 2
);

/**
 * Checks availability of cloud services on startup to control UI functionality.
 *
 * At client startup, verifies which services are supported by the specific cloud instance.
 * Client responds to the supported services list by enabling/disabling corresponding UI elements.
 * Service availability depends on infrastructure capabilities and instance-specific configuration.
 * For example, Private Cloud lacks push_notifications service, so the client should not display
 * Mobile Notification Actions in UI.
 */
class NX_VMS_CLIENT_CORE_API CloudServiceChecker: public QObject
{
    Q_OBJECT

public:
    Q_DECLARE_FLAGS(CloudServices, CloudService)
    explicit CloudServiceChecker(QObject* parent = nullptr);
    virtual ~CloudServiceChecker() override;

    bool hasService(CloudService service) const;

signals:
    void supportedServicesChanged(CloudServices changedServices);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
