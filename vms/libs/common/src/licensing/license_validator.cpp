#include "license_validator.h"

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <licensing/license.h>
#include <utils/common/synctime.h>

#include <nx/vms/api/types/connection_types.h>

using namespace nx;

QnLicenseValidator::QnLicenseValidator(QnCommonModule* commonModule, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(commonModule)
{

}

QnLicenseValidator::~QnLicenseValidator()
{

}

bool QnLicenseValidator::isValid(const QnLicensePtr& license, ValidationMode mode) const
{
    return validate(license, mode) == QnLicenseErrorCode::NoError;
}

QnLicenseErrorCode QnLicenseValidator::validate(const QnLicensePtr& license,
    ValidationMode mode) const
{
    /**
     * >= v1.5, shoud have hwid1, hwid2 or hwid3, and have brand
     * v1.4 license may have or may not have brand, depending on was activation was done before or after 1.5 is released
     * We just allow empty brand for all, because we believe license is correct.
     */
    if (!license->isValidSignature() && mode != VM_CanActivate)
        return QnLicenseErrorCode::InvalidSignature;

    const auto& manager = runtimeInfoManager();
    QnPeerRuntimeInfo info = manager->items()->getItem(mode == VM_Regular
        ? serverId(license)
        : commonModule()->remoteGUID());

    // #TODO: #ynikitenkov It does not make sense in case of VM_JustAdded. #refactor
    // peer where license was activated not found
    if (info.uuid.isNull())
        return QnLicenseErrorCode::InvalidHardwareID;

    if (!license->brand().isEmpty() && license->brand() != info.data.brand)
        return QnLicenseErrorCode::InvalidBrand;

    // TODO: #rvasilenko make NEVER an INT64_MAX
    if (license->expirationTime() > 0 && qnSyncTime->currentMSecsSinceEpoch() > license->expirationTime())
        return QnLicenseErrorCode::Expired;

    if (license->type() == Qn::LC_Start)
        return isValidStartLicense(license, mode);

    if (license->type() == Qn::LC_Invalid)
        return QnLicenseErrorCode::FutureLicense;

    return QnLicenseErrorCode::NoError;
}

QString QnLicenseValidator::validationInfo(const QnLicensePtr& license, ValidationMode mode) const
{
    QnLicenseErrorCode code = validate(license, mode);
    if (code == QnLicenseErrorCode::NoError)
        return lit("Ok");

    return errorMessage(code);
}

QString QnLicenseValidator::errorMessage(QnLicenseErrorCode errCode)
{
    switch (errCode)
    {
        case QnLicenseErrorCode::NoError:
            return QString();
        case QnLicenseErrorCode::InvalidSignature:
            return tr("Invalid signature");
        case QnLicenseErrorCode::InvalidHardwareID:
            return tr("Server with matching Hardware ID not found");
        case QnLicenseErrorCode::InvalidBrand:
            return tr("Invalid customization");
        case QnLicenseErrorCode::Expired:
            return tr("License is expired"); // license is out of date
        case QnLicenseErrorCode::InvalidType:
            return tr("Invalid type");
        case QnLicenseErrorCode::TooManyLicensesPerDevice:
            return tr("Only one starter license is allowed per System.")
                + '\n'
                + tr("You already have one active starter license.");
        case QnLicenseErrorCode::FutureLicense:
            return tr("This license type requires higher software version");
        default:
            return tr("Unknown error");
    }

    return QString();
}

QnUuid QnLicenseValidator::serverId(const QnLicensePtr& license) const
{
    for (const QnPeerRuntimeInfo& info: runtimeInfoManager()->items()->getItems())
    {
        if (info.data.peer.peerType != vms::api::PeerType::server)
            continue;

        bool hwKeyOK = info.data.hardwareIds.contains(license->hardwareId());
        bool brandOK = license->brand().isEmpty() || (license->brand() == info.data.brand);
        if (hwKeyOK && brandOK)
            return info.uuid;
    }
    return QnUuid();
}

QnLicenseErrorCode QnLicenseValidator::isValidStartLicense(const QnLicensePtr& license,
    ValidationMode mode) const
{
    // Only single Start license per system is allowed
    for (const QnLicensePtr& otherLicense: licensePool()->getLicenses())
    {
        // Skip other license types and current license itself.
        if (otherLicense->type() != license->type() || otherLicense->key() == license->key())
            continue;

        // Do not allow to activate new start license when there is one already.
        if (mode == ValidationMode::VM_CanActivate)
            return QnLicenseErrorCode::TooManyLicensesPerDevice;

        // Skip other licenses if they have less channels.
        if (otherLicense->cameraCount() < license->cameraCount())
            continue;

        // Mark current as invalid if it has less channels.
        if (otherLicense->cameraCount() > license->cameraCount())
            return QnLicenseErrorCode::TooManyLicensesPerDevice;

        // We found another license with the same number of channels.
        // Mark the least license as valid.
        if (otherLicense->key() < license->key())
            return QnLicenseErrorCode::TooManyLicensesPerDevice;
    }

    return QnLicenseErrorCode::NoError;
}
