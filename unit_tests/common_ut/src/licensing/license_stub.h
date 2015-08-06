#ifndef QN_LICENSE_STUB_H
#define QN_LICENSE_STUB_H

#include <licensing/license.h>

class QnLicenseStub: public QnLicense {
public:
    QnLicenseStub(Qn::LicenseType licenseType, int count);

    virtual bool isValid(ErrorCode* errCode /* = 0 */, ValidationMode mode /* = VM_Regular */) const;

    virtual Qn::LicenseType type() const override;

    bool isArmServer() const;
    void setArmServer(bool value);
private:
    Qn::LicenseType m_type;

    /** Should we count current server as ARM when validating. */
    bool m_armServer;
};

#endif //QN_LICENSE_STUB_H
