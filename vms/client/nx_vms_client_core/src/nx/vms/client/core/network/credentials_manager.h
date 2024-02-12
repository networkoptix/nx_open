// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

/**
 * Centralized interface for working with stored credentials.
 * Username is stored as-is but search is case-insensitive.
 */
struct NX_VMS_CLIENT_CORE_API CredentialsManager: public QObject
{
    using Credentials = nx::network::http::Credentials;

    CredentialsManager(QObject* parent = nullptr);

    /** Store provided credentials to persistent settings. */
    static void storeCredentials(const nx::Uuid& localSystemId, const Credentials& credentials);

    /** Remove credentials of the single user. */
    static void removeCredentials(const nx::Uuid& localSystemId, const std::string& user);

    /** Remove all stored credentials for the system. */
    static void removeCredentials(const nx::Uuid& localSystemId);

    /** Cleanup stored user password, leaving user intact. */
    static void forgetStoredPassword(const nx::Uuid& localSystemId, const std::string& user);

    /** Cleanup all stored user passwords, leaving users intact. */
    static void forgetStoredPasswords();

    /** Find all stored credentials for the given system.*/
    static std::vector<Credentials> credentials(const nx::Uuid& localSystemId);

    /** Find stored credentials for the given user in the given system. */
    static std::optional<Credentials> credentials(
        const nx::Uuid& localSystemId,
        const std::string& user);

private:
    Q_OBJECT

signals:
    void storedCredentialsChanged();
};

} // namespace nx::vms::client::core {
