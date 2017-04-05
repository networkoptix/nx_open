#pragma once

#include <licensing/license.h>

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
