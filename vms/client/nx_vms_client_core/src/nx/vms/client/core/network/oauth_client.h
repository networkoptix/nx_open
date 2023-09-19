// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

struct CloudAuthData;

/**
 * Implements logic for OAuth authentication process. Constructs URL to be used for the
 * requests for the cloud login/2fa/etc pages and processes responses from them.
 */
class NX_VMS_CLIENT_CORE_API OauthClient: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QUrl url READ url CONSTANT)

public:
    static void registerQmlType();

    OauthClient(QObject* parent = nullptr);

    /**
     * Construct OAuth client with specified parameters.
     * @param clientType Action to be executed, like login to cloud/2FA validation/etc.
     * @param viewType Type of the visual representation - whether it is mobile or desktop view.
     * @param user Optional user name to be used for authorization process
     * @param cloudSystem Optional cloud system id to define the scope of the token.
     * @param parent Parent object.
     */
    OauthClient(
        OauthClientType clientType,
        OauthViewType viewType,
        const QString& cloudSystem = QString(),
        QObject* parent = nullptr);

    virtual ~OauthClient() override;

    /** Constructed URL to request cloud web page. */
    QUrl url() const;

    /** Set token and username for 2FA validation. */
    void setCredentials(const nx::network::http::Credentials& credentials);

    /** Set locale code for URL construction. */
    void setLocale(const QString& locale);

    /** Final authetication data. */
    const CloudAuthData& authData() const;

    /** Set code returned by cloud on login to cloud operation. */
    Q_INVOKABLE void setCode(const QString& code);

    /** Set code after 2FA verification. */
    Q_INVOKABLE void twoFaVerified(const QString& code);

signals:
    void authDataReady();
    void cancelled();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
