#include "license_stub.h"

QnLicenseStub::QnLicenseStub(Qn::LicenseType licenseType, int count):
    m_type(licenseType)
{
   setKey(QnUuid::createUuid().toByteArray());
   setCameraCount(count);
}

bool QnLicenseStub::isValid(ErrorCode* errCode /* = 0 */, ValidationMode mode /* = VM_Regular */) const {
    return true;
}

Qn::LicenseType QnLicenseStub::type() const  {
    return m_type;
}
