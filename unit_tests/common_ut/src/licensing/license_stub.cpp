#include "license_stub.h"

static QUuid initialKey = QUuid::createUuid();


QnLicenseStub::QnLicenseStub(Qn::LicenseType licenseType, int count):
    m_type(licenseType)
{
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    setCameraCount(count);
}

bool QnLicenseStub::isValid(ErrorCode* /*errCode = 0 */,
                            ValidationMode /*mode = VM_Regular */) const {
    // Only single Start license per system is allowed
    if (type() == Qn::LC_Start)
        return isValidStartLicense();

    return true;
}

Qn::LicenseType QnLicenseStub::type() const  {
    return m_type;
}
