// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "list_helper.h"

#include <licensing/license.h>

#include "validator.h"

namespace nx::vms::license {

static std::array<int, Qn::LicenseType::LC_Count> g_fakeLicenseCount;

ListHelper::ListHelper(const QnLicenseList& licenseList)
{
    update(licenseList);
}

bool ListHelper::haveLicenseKey(const QByteArray& key) const
{
    return m_licenseDict.contains(key);
}

QnLicensePtr ListHelper::getLicenseByKey(const QByteArray& key) const
{
    if (m_licenseDict.contains(key))
        return m_licenseDict[key];
    else
        return QnLicensePtr();
}

QList<QByteArray> ListHelper::allLicenseKeys() const
{
    return m_licenseDict.keys();
}

int ListHelper::totalLicenseByType(Qn::LicenseType licenseType,
    const Validator* validator) const
{
    if (licenseType == Qn::LC_Free)
        return std::numeric_limits<int>::max();

    int result = g_fakeLicenseCount[(size_t) licenseType];

    for (const QnLicensePtr& license: m_licenseDict.values())
    {
        if (license->type() != licenseType)
            continue;

        if (validator && !validator->isValid(license))
            continue;

        result += license->cameraCount();
    }
    return result;
}

void ListHelper::update(const QnLicenseList& licenseList)
{
    m_licenseDict.clear();
    for (const auto& license: licenseList)
        m_licenseDict[license->key()] = license;
}

void setFakeLicenseCount(int value, Qn::LicenseType licenseType)
{
    g_fakeLicenseCount[licenseType] = value;
}

} // namespace nx::vms::license
