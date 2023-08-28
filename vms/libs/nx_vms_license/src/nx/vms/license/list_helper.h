// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/vms/license/license_usage_fwd.h>

namespace nx::vms::license {

class ListHelper
{
public:
    ListHelper() = default;
    ListHelper(const QnLicenseList& licenseList);

    void update(const QnLicenseList& licenseList);

    QList<QByteArray> allLicenseKeys() const;
    bool haveLicenseKey(const QByteArray& key) const;
    QnLicensePtr getLicenseByKey(const QByteArray& key) const;

    /** If validator is passed, only valid licenses are counted. */
    int totalLicenseByType(Qn::LicenseType licenseType, const Validator* validator) const;

private:
    QnLicenseDict m_licenseDict;
};

// Should only be used for UTs.
void setFakeLicenseCount(int value, Qn::LicenseType licenseType);

} // namespace nx::vms::license
