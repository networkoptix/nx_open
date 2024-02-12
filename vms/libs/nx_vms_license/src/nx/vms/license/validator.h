// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <licensing/license_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

struct QnPeerRuntimeInfo;

namespace nx::vms::license {

enum class QnLicenseErrorCode
{
    NoError,
    InvalidSignature,           /**< License digital signature is not match */
    InvalidHardwareID,          /**< Invalid hardware ID */
    InvalidBrand,               /**< License belong to other customization */
    Expired,                    /**< Expired */
    InvalidType,                /**< Such license type isn't allowed for that device. */
    TooManyLicensesPerSystem,   /**< Too many licenses of this type per system. */
    FutureLicense,              /**< License type is unknown, may be license from future version. */
    TemporaryExpired            /**< License is not validated in time. Temporary expiration date passed. */
};

// TODO: #sivanov Store Context pointer in the License itself and use it.
class Validator:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum ValidationMode
    {
        VM_Regular,
        VM_CanActivate,
        VM_JustCreated
    };

    Validator(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~Validator();

    /**
    * Check if signature matches other fields, also check hardwareId and brand
    */
    bool isValid(const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;

    /**
     * Check if signature matches other fields, also check hardwareId and brand
     */
    virtual QnLicenseErrorCode validate(
        const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;

    /**
     * Return true to use different runtime info if the server is missing.
     */
    virtual bool overrideMissingRuntimeInfo(const QnLicensePtr& license, QnPeerRuntimeInfo& info) const;

    QString validationInfo(const QnLicensePtr& license, ValidationMode mode = VM_Regular) const;

    static QString errorMessage(QnLicenseErrorCode errCode, Qn::LicenseType licenseType);

    /** Id of the server this license attached to (if it is present in the current system). */
    nx::Uuid serverId(const QnLicensePtr& license) const;

protected:
    /**
     * Check if only one license of this type is allowed to exist in the system.
     */
    QnLicenseErrorCode isValidUniqueLicense(const QnLicensePtr& license,
        ValidationMode mode = VM_Regular) const;
};

} // namespace nx::vms::license
