// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <licensing/license.h>
#include <nx/vms/license/validator.h>

namespace nx::vms::license::test {

class QnLicenseStub: public QnLicense
{
public:
    QnLicenseStub(Qn::LicenseType licenseType, int count);

    virtual Qn::LicenseType type() const override;

private:
    Qn::LicenseType m_type;
};

class QnFutureLicenseStub: public QnLicense
{
public:
    QnFutureLicenseStub(int count);
};

class QLicenseStubValidator: public Validator
{
    using base_type = Validator;
public:
    QLicenseStubValidator(nx::vms::common::SystemContext* context, QObject* parent = nullptr);

    virtual QnLicenseErrorCode validate(const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;
};

} // namespace nx::vms::license::test
