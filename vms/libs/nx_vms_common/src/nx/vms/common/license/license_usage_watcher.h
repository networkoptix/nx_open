// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <licensing/license.h>

class QnRuntimeInfoManager;

namespace nx::vms::common {

class NX_VMS_COMMON_API LicenseUsageWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LicenseUsageWatcher(QnLicensePool* licensePool, QnRuntimeInfoManager* runtimeInfoManager, QObject* parent = nullptr);

signals:
    void licenseUsageChanged();
};

class NX_VMS_COMMON_API DeviceLicenseUsageWatcher final: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    DeviceLicenseUsageWatcher(QnLicensePool* licensePool, QnRuntimeInfoManager* runtimeInfoManager, QnResourcePool* resourcePool, QObject* parent = nullptr);
};

class NX_VMS_COMMON_API VideoWallLicenseUsageWatcher final: public LicenseUsageWatcher
{
    Q_OBJECT
    using base_type = LicenseUsageWatcher;

public:
    VideoWallLicenseUsageWatcher(QnLicensePool* licensePool, QnRuntimeInfoManager* runtimeInfoManager, QnResourcePool* resourcePool, QObject* parent = nullptr);
};

} // namespace nx::vms::common
