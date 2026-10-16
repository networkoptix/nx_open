// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::core { class OauthClient; }

namespace nx::vms::client::mobile::detail {

class CredentialsHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool hasDigestCloudPassword
        READ hasDigestCloudPassword
        NOTIFY digestCloudPasswordChanged)

public:
    CredentialsHelper(QObject* parent = nullptr);

    Q_INVOKABLE void removeSavedConnection(
        const QString& systemId,
        const QString& localSystemId,
        const QString& userName = {});

    Q_INVOKABLE void removeSavedCredentials(
        const QString& localSystemId,
        const QString& userName);

    Q_INVOKABLE void removeSavedAuth(
        const QString& localSystemId,
        const QString& userName);

    Q_INVOKABLE void clearSavedPasswords();

    /**
     * Create Oauth client with the specified token and optional user. A 2FA validation client
     * only validates the token and does not produce new auth data.
     */
    Q_INVOKABLE nx::vms::client::core::OauthClient* createOauthClient(
        const QString& token, const QString& user, bool twoFactorValidation = false) const;

    /**
     * Apply auth data received by the Oauth client to CloudStatusWatcher. Returns false if there
     * is no auth data.
     */
    Q_INVOKABLE bool applyAuthData(nx::vms::client::core::OauthClient* client, bool forced) const;

    bool hasDigestCloudPassword() const;

signals:
    void digestCloudPasswordChanged();
};

} // namespace nx::vms::client::mobile::detail
