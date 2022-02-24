// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //< For Q_DECLARE_TR_FUNCTIONS.

#include <licensing/license_fwd.h>
#include <nx/vms/license/license_usage_fwd.h>
#include <common/common_globals.h>

#include <nx/vms/client/desktop/license/license_helpers.h>

namespace ec2 { enum class ErrorCode; }

namespace nx::vms::client::desktop {

/**
 * Display license activation message boxes with different errors.
 */
class LicenseActivationDialogs
{
    Q_DECLARE_TR_FUNCTIONS(LicenseActivationDialogs)

public:
    /** Warn about an incompatible license when activating it. */
    static void licenseIsIncompatible(QWidget* parent);

    /** Warn about corrupted license data while activating a license. */
    static void invalidDataReceived(QWidget* parent);

    /** Warn about internal db error while activating a license. */
    static void databaseErrorOccurred(QWidget* parent);

    /** Warn about an incorrectly entered license key while activating a license. */
    static void invalidLicenseKey(QWidget* parent);

    /** Warn about an incorrect manual activation key file. */
    static void invalidKeyFile(QWidget* parent);

    /** Warn about the license is already activated on the given hardware id at the given time. */
    static void licenseAlreadyActivated(
        QWidget* parent,
        const QString& hardwareId,
        const QString& time = QString());

    /** Notify about this license is already activated in the current system. */
    static void licenseAlreadyActivatedHere(QWidget* parent);

    /** Warn about failed activation. */
    static void activationError(QWidget* parent,
        nx::vms::license::QnLicenseErrorCode errorCode,
        Qn::LicenseType licenseType);

    /** Warn about a network error while activating a license. */
    static void networkError(QWidget* parent);

     /** Notify about successful license activation. */
    static void success(QWidget* parent);
};

/**
 * Display license deactivation message boxes with different errors.
 */
class LicenseDeactivationDialogs
{
    Q_DECLARE_TR_FUNCTIONS(LicenseDeactivationDialogs)

public:
    /**
     * Warn about failed deactivation. No licenses can bbe deactivated.
     */
    static void deactivationError(
        QWidget* parent,
        const QnLicenseList& licenses,
        const license::DeactivationErrors& errors);

    /**
     * Warn about partially failed deactivation, when some licenses still can be deactivated.
     * @returns true if user still wants to deactivate licenses he is able to.
     */
    static bool partialDeactivationError(
        QWidget* parent,
        const QnLicenseList& licenses,
        const license::DeactivationErrors& errors);

    /** Warn about an unexpected deactivation error. */
    static void unexpectedError(QWidget* parent, const QnLicenseList& licenses);

    /** Notify about connection problems when deactivating licenses. */
    static void networkError(QWidget* parent);

    /** Warn about licenes server error while deactivating licenses. */
    static void licensesServerError(QWidget* parent, const QnLicenseList& licenses);

    /** Warn about failure while removing deactivated license. */
    static void failedToRemoveLicense(QWidget* parent, ec2::ErrorCode errorCode);

    /** Notify about successful license deactivation. */
    static void success(QWidget* parent, const QnLicenseList& licenses);
};

} // namespace nx::vms::client::desktop
