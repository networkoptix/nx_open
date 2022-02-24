// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>

class QnLicensePool;

namespace nx::vms::license::test {

/** RAII class for managing license pool. */
class QnLicensePoolScaffold
{
public:
    QnLicensePoolScaffold(QnLicensePool* licensePool);
    ~QnLicensePoolScaffold();

    void addLicense(Qn::LicenseType licenseType);
    void addLicenses(Qn::LicenseType licenseType, int count);

    void addFutureLicenses(int count);

    void setArmMode(bool isArm = true);
private:
    QnLicensePool* const m_licensePool {nullptr};
    /** Are we emulating the arm server. */
    bool m_arm {false};
};

} // namespace nx::vms::license::test
