// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(RemoteConnectionErrorCode,

    /** Binary protocol is used, and its version differs. */
    binaryProtocolVersionDiffers,

    /** User rejected the provided handshake certificate. */
    certificateRejected,

    /** Server has different cloud host. */
    cloudHostDiffers,

    /** Cloud session token expired. */
    cloudSessionExpired,

    /** Cloud is not available on the Client side. */
    cloudUnavailableOnClient,

    /** Cloud is not available on the Server side. */
    cloudUnavailableOnServer,

    /** Connection timed out. */
    connectionTimeout,

    /** Server has different customization. */
    customizationDiffers,

    /** Server is not setup yet. */
    factoryServer,

    /** Server replied with http 'forbidden' error code. */
    forbiddenRequest,

    /** Unspecified network error. */
    genericNetworkError,

    /** Client internal error. Should never happen. */
    internalError,

    /** Authorization failed as LDAP server connection is not established. */
    ldapInitializationInProgress,

    /** Login as cloud user is forbidden. */
    loginAsCloudUserForbidden,

    /** Server replied with invalid content. */
    networkContentError,

    /** Saved or actual local session token expired. */
    sessionExpired,

    /** Temporary token expired, session token can not be created. */
    temporaryTokenExpired,

    /** System is not compatible with two-factor authentication. */
    systemIsNotCompatibleWith2Fa,

    /** Two-factor authentication must be enabled for both the system and the user. */
    twoFactorAuthOfCloudUserIsDisabled,

    /** Authorization failed. */
    unauthorized,

    /** Server id is not the same that we expected (e.g. when restoring client session). */
    unexpectedServerId,

    /** User is disabled. */
    userIsDisabled,

    /** User is temporary locked out. */
    userIsLockedOut,

    /** Server version is too low. */
    versionIsTooLow
);

struct NX_VMS_CLIENT_CORE_API RemoteConnectionError
{
    RemoteConnectionErrorCode code;
    /** Description from the server side. */
    std::optional<QString> externalDescription;

    RemoteConnectionError(RemoteConnectionErrorCode code);
    bool operator==(const RemoteConnectionError&) const = default;
    bool operator==(RemoteConnectionErrorCode) const;

    /** Debug representation. */
    QString toString() const;
};

} // namespace nx::vms::client::core
