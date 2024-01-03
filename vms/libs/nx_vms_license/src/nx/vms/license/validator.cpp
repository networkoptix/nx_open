// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "validator.h"

#include <api/runtime_info_manager.h>
#include <licensing/license.h>
#include <nx/branding.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/synctime.h>

namespace nx::vms::license {

Validator::Validator(nx::vms::common::SystemContext* context, QObject* parent):
    base_type(parent),
    nx::vms::common::SystemContextAware(context)
{
}

Validator::~Validator()
{

}

bool Validator::isValid(const QnLicensePtr& license, ValidationMode mode) const
{
    return validate(license, mode) == QnLicenseErrorCode::NoError;
}

bool Validator::overrideMissingRuntimeInfo(const QnLicensePtr&, QnPeerRuntimeInfo&) const
{
    return false;
}

QnLicenseErrorCode Validator::validate(const QnLicensePtr& license, ValidationMode mode) const
{
    /**
     * >= v1.5, should have hwid1, hwid2 or hwid3, and have brand
     * v1.4 license may have or may not have brand, depending on was activation was done before or after 1.5 is released
     * We just allow empty brand for all, because we believe license is correct.
     */
    if (!license->isValidSignature() && mode != VM_CanActivate)
        return QnLicenseErrorCode::InvalidSignature;

    QnUuid currentServerId;
    auto connection = messageBusConnection();
    if (connection)
        currentServerId = connection->moduleInformation().id;

    const auto& manager = m_context->runtimeInfoManager();
    QnPeerRuntimeInfo info;

    if (license->type() == Qn::LC_SaasLocalRecording)
    {
        const auto date = license->tmpExpirationDate();
        if (date.isValid() && date.toMSecsSinceEpoch() < qnSyncTime->currentMSecsSinceEpoch())
            return QnLicenseErrorCode::TemporaryExpired;
        info = manager->items()->getItem(currentServerId);
    }
    else
    {
        // Check local license by hardwareId.
        info = manager->items()->getItem(mode == VM_Regular ? serverId(license) : currentServerId);

        // #TODO: #ynikitenkov It does not make sense in case of VM_JustAdded.
        // Peer where license was activated was not found.
        if (info.uuid.isNull() && !overrideMissingRuntimeInfo(license, info))
            return QnLicenseErrorCode::InvalidHardwareID;
    }

    if (!license->brand().isEmpty() && license->brand() != info.data.brand)
        return QnLicenseErrorCode::InvalidBrand;

    // TODO: #rvasilenko make NEVER an INT64_MAX
    if (license->expirationTime() > 0 && qnSyncTime->currentMSecsSinceEpoch() > license->expirationTime())
        return QnLicenseErrorCode::Expired;

    if (license->isUniqueLicenseType())
        return isValidUniqueLicense(license, mode);

    if (license->type() == Qn::LC_Invalid)
        return QnLicenseErrorCode::FutureLicense;

    return QnLicenseErrorCode::NoError;
}

QString Validator::validationInfo(const QnLicensePtr& license, ValidationMode mode) const
{
    QnLicenseErrorCode code = validate(license, mode);
    if (code == QnLicenseErrorCode::NoError)
        return "Ok";

    return errorMessage(code, license->type());
}

QString Validator::errorMessage(QnLicenseErrorCode errCode, Qn::LicenseType licenseType)
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
        case QnLicenseErrorCode::TemporaryExpired:
            // License is out of date temporary.
            return tr("License is not validated by %1", "%1 is the short cloud name (like Cloud)")
                .arg(nx::branding::shortCloudName());
        case QnLicenseErrorCode::InvalidType:
            return tr("Invalid type");
        case QnLicenseErrorCode::TooManyLicensesPerSystem:
            switch (licenseType)
            {
                case Qn::LC_Start:
                    return tr("Only one Starter license is allowed per Site.")
                        + '\n'
                        + tr("You already have one active Starter license.");
                case Qn::LC_Nvr:
                    return tr("Only one NVR license is allowed per Site.")
                        + '\n'
                        + tr("You already have one active NVR license.");
                default:
                    return tr("Only one license of this type is allowed per Site.")
                        + '\n'
                        + tr("You already have one active license of the same type.");
            }
        case QnLicenseErrorCode::FutureLicense:
            return tr("This license type requires higher software version");
        default:
            return tr("Unknown error");
    }

    return QString();
}

QnUuid Validator::serverId(const QnLicensePtr& license) const
{
    const auto items = m_context->runtimeInfoManager()->items()->getItems();
    for (const QnPeerRuntimeInfo& info: items)
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

QnLicenseErrorCode Validator::isValidUniqueLicense(const QnLicensePtr& license,
    ValidationMode mode) const
{
    // Only single license of this type per system is allowed.
    for (const QnLicensePtr& otherLicense: m_context->licensePool()->getLicenses())
    {
        // Skip other license types and current license itself.
        if (otherLicense->type() != license->type() || otherLicense->key() == license->key())
            continue;

        // Do not allow to activate new start or nvr license when there is one already.
        if (mode == ValidationMode::VM_CanActivate)
            return QnLicenseErrorCode::TooManyLicensesPerSystem;

        // Skip other licenses if they have less channels.
        if (otherLicense->cameraCount() < license->cameraCount())
            continue;

        // Mark current as invalid if it has less channels.
        if (otherLicense->cameraCount() > license->cameraCount())
            return QnLicenseErrorCode::TooManyLicensesPerSystem;

        // We found another license with the same number of channels.
        // Mark the least license as valid.
        if (otherLicense->key() < license->key())
            return QnLicenseErrorCode::TooManyLicensesPerSystem;
    }

    return QnLicenseErrorCode::NoError;
}

} // namespace nx::vms::license
