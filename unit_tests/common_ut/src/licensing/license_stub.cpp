#include "license_stub.h"

static QUuid initialKey = QUuid::createUuid();

/************************************************************************/
/* QnLicenseStub                                                        */
/************************************************************************/

QnLicenseStub::QnLicenseStub(Qn::LicenseType licenseType, int count):
    m_type(licenseType),
    m_armServer(false)
{
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    setCameraCount(count);
}

// bool QnLicenseStub::isValid(ErrorCode* /*errCode = 0 */, ValidationMode /* mode = VM_Regular */) const {
//     if (isArmServer() && !isAllowedForArm())
//         return false;
//
//     // Only single Start license per system is allowed
//     if (type() == Qn::LC_Start)
//         return isValidStartLicense();
//
//     return true;
// }

Qn::LicenseType QnLicenseStub::type() const  {
    return m_type;
}

bool QnLicenseStub::isArmServer() const {
    return m_armServer;
}

void QnLicenseStub::setArmServer(bool value) {
    m_armServer = value;
}

/************************************************************************/
/* QnFutureLicenseStub                                                  */
/************************************************************************/

QnFutureLicenseStub::QnFutureLicenseStub(int count) {
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    setClass("some-future-class");
    setCameraCount(count);
}
//
// bool QnFutureLicenseStub::isValid(
//     ErrorCode* /*errCode  = 0 */,
//     ValidationMode /* mode = VM_Regular */) const
// {
//     return true;
// }
