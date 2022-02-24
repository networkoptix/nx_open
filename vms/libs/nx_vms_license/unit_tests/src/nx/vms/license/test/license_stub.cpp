// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_stub.h"

namespace {

static QUuid initialKey = QUuid::createUuid();

} // namespace

namespace nx::vms::license::test {

QnLicenseStub::QnLicenseStub(Qn::LicenseType licenseType, int count):
    m_type(licenseType),
    m_armServer(false)
{
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    setCameraCount(count);
}

Qn::LicenseType QnLicenseStub::type() const
{
    return m_type;
}

bool QnLicenseStub::isArmServer() const
{
    return m_armServer;
}

void QnLicenseStub::setArmServer(bool value)
{
    m_armServer = value;
}

QnFutureLicenseStub::QnFutureLicenseStub(int count)
{
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    setClass("some-future-class");
    setCameraCount(count);
}

QLicenseStubValidator::QLicenseStubValidator(QnCommonModule* commonModule, QObject* parent):
    base_type(commonModule, parent)
{
}

QnLicenseErrorCode QLicenseStubValidator::validate(const QnLicensePtr& license,
    ValidationMode mode) const
{
    auto futureStub = dynamic_cast<QnFutureLicenseStub*>(license.data());
    if (futureStub)
        return QnLicenseErrorCode::NoError;

    auto stub = dynamic_cast<QnLicenseStub*>(license.data());
    if (stub)
    {
        if (license->isUniqueLicenseType())
            return isValidUniqueLicense(license);

        return QnLicenseErrorCode::NoError;
    }

    return base_type::validate(license, mode);
}

} // namespace nx::vms::license::test
