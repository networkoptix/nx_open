#pragma once

#include <licensing/license.h>
#include <licensing/license_validator.h>

class QnLicenseStub: public QnLicense
{
public:
    QnLicenseStub(Qn::LicenseType licenseType, int count);

    virtual Qn::LicenseType type() const override;

    bool isArmServer() const;
    void setArmServer(bool value);

private:
    Qn::LicenseType m_type;

    /** Should we count current server as ARM when validating. */
    bool m_armServer;
};

class QnFutureLicenseStub: public QnLicense
{
public:
    QnFutureLicenseStub(int count);
};

class QLicenseStubValidator: public QnLicenseValidator
{
    using base_type = QnLicenseValidator;
public:
    QLicenseStubValidator(QObject* parent = nullptr);

    virtual QnLicenseErrorCode validate(const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;
};
