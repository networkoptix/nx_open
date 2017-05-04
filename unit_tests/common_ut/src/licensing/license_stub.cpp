#include "license_stub.h"

static QUuid initialKey = QUuid::createUuid();

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

QLicenseStubValidator::QLicenseStubValidator(QObject* parent):
    base_type(parent)
{
}

QnLicenseErrorCode QLicenseStubValidator::validate(const QnLicensePtr& license, ValidationMode mode) const
{
    auto futureStub = dynamic_cast<QnFutureLicenseStub*>(license.data());
    if (futureStub)
        return QnLicenseErrorCode::NoError;

    auto stub = dynamic_cast<QnLicenseStub*>(license.data());
    if (stub)
    {
        if (stub->isArmServer() && !isAllowedForArm(license))
            return QnLicenseErrorCode::InvalidType;

        // Only single Start license per system is allowed
        if (license->type() == Qn::LC_Start)
            return isValidStartLicense(license);

        return QnLicenseErrorCode::NoError;
    }

    return base_type::validate(license, mode);
}
