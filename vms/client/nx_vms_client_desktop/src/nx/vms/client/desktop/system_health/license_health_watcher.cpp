// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_health_watcher.h"

#include <licensing/license.h>
#include <nx/vms/license/validator.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

LicenseHealthWatcher::LicenseHealthWatcher(
    QnLicensePool* licensePool,
    QObject* parent)
:
    base_type(parent),
    m_licensePool(licensePool)
{
    connect(&m_timer, &QTimer::timeout, this, &LicenseHealthWatcher::at_timer);
    m_timer.start(1000 * 60);
}

void LicenseHealthWatcher::at_timer()
{
    nx::vms::license::Validator validator(m_licensePool->context());
    for (const auto& license: m_licensePool->getLicenses())
    {
        if (validator.validate(license) == nx::vms::license::QnLicenseErrorCode::Expired)
        {
            qint64 expirationDelta = qnSyncTime->currentMSecsSinceEpoch() - license->expirationTime();
            if (expirationDelta < m_timer.interval())
            {
                emit m_licensePool->licensesChanged();
                break;
            }
        }
    }
}

} // namespace nx::vms::client::desktop
