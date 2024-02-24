// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API LicenseUsageWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LicenseUsageWatcher(SystemContext* context, QObject* parent = nullptr);

signals:
    void licenseUsageChanged();
};

class NX_VMS_COMMON_API DeviceLicenseUsageWatcher: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    DeviceLicenseUsageWatcher(SystemContext* context, QObject* parent = nullptr);
};

class NX_VMS_COMMON_API VideoWallLicenseUsageWatcher: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    VideoWallLicenseUsageWatcher(SystemContext* context, QObject* parent = nullptr);
};

} // namespace nx::vms::common
