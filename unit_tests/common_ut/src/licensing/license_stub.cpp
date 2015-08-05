#include "license_stub.h"

static QUuid initialKey = QUuid::createUuid();


QnLicenseStub::QnLicenseStub(Qn::LicenseType licenseType, int count):
    m_type(licenseType)
{
    ++initialKey.data1;
    setKey(initialKey.toByteArray());
    qDebug() << key();
    setCameraCount(count);
}

bool QnLicenseStub::isValid(ErrorCode* /*errCode = 0 */,
                            ValidationMode /*mode = VM_Regular */) const {
    // Only single Start license per system is allowed
    if (type() == Qn::LC_Start)
    {
        for(const QnLicensePtr& license: qnLicensePool->getLicenses()) {
            // skip other licenses and current license itself
            if (license->type() != type() || license->key() == key())
                continue;

            // skip other licenses if they have less channels
            if (license->cameraCount() < cameraCount())
                continue;

            if (license->cameraCount() > cameraCount())
                return false; // mark current as invalid if it has less channels

            // we found another license with the same number of channels
            if (license->key() < key())
                return false; // mark the most least license as valid
        }
    }

    return true;
}

Qn::LicenseType QnLicenseStub::type() const  {
    return m_type;
}
