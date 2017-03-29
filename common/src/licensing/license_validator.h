#pragma once

#include <QtCore/QObject>

#include <licensing/license_fwd.h>

#include <common/common_module_aware.h>

#include <nx/utils/uuid.h>

enum class QnLicenseErrorCode
{
    NoError,
    InvalidSignature,           /**< License digital signature is not match */
    InvalidHardwareID,          /**< Invalid hardware ID */
    InvalidBrand,               /**< License belong to other customization */
    Expired,                    /**< Expired */
    InvalidType,                /**< Such license type isn't allowed for that device. */
    TooManyLicensesPerDevice,   /**< Too many licenses of this type per device. */
    FutureLicense               /**< License type is unknown, may be license from future version. */
};

class QnLicenseValidator: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum ValidationMode
    {
        VM_Regular,
        VM_CheckInfo,
        VM_JustCreated
    };

    QnLicenseValidator(QObject* parent = nullptr);
    virtual ~QnLicenseValidator();

    /**
     * Check if signature matches other fields, also check hardwareId and brand
     */
    bool isValid(const QnLicensePtr& license, ValidationMode mode = VM_Regular,
        QnLicenseErrorCode* errCode = nullptr) const;

    QString validationInfo(const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;

    static QString errorMessage(QnLicenseErrorCode errCode);

    /** Id of the server this license attached to (if it is present in the current system). */
    QnUuid serverId(const QnLicensePtr& license) const;

private:
    bool isValidEdgeLicense(const QnLicensePtr& license, QnLicenseErrorCode* errCode = 0,
        ValidationMode mode = VM_Regular) const;
    bool isValidStartLicense(const QnLicensePtr& license, QnLicenseErrorCode* errCode = 0) const;
    bool isAllowedForArm(const QnLicensePtr& license) const;

    bool gotError(QnLicenseErrorCode* errCode, QnLicenseErrorCode errorCodeValue) const;
};
