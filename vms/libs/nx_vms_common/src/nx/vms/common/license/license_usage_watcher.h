// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API LicenseUsageWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LicenseUsageWatcher(QnCommonModule* context, QObject* parent = nullptr);

signals:
    void licenseUsageChanged();
};

class NX_VMS_COMMON_API DeviceLicenseUsageWatcher: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    DeviceLicenseUsageWatcher(QnCommonModule* context, QObject* parent = nullptr);
};

class NX_VMS_COMMON_API VideoWallLicenseUsageWatcher: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    VideoWallLicenseUsageWatcher(QnCommonModule* context, QObject* parent = nullptr);
};

} // namespace nx::vms::common
