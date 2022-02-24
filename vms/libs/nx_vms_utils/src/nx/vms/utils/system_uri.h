// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Classes for handling and constructing generic URI protocol links which use Nx URL Scheme.
 *
 * Full scheme looks like this:
 * ```
 * {protocol}://{domain}/{client_command}/{system_id}{system_action}?auth={access_key}&{action_parameters}
 * ```
 *
 * Protocol part is customizable, so we don't parse it here. Auth is encoded as base64(login:pass).
 * All query parameters are url-encoded.
 */

#include <QtCore/QScopedPointer>
#include <QtCore/QHash>

class QnUuid;

namespace nx::utils { class Url; };

namespace nx::vms::utils {

class SystemUriPrivate;

class NX_VMS_UTILS_API SystemUri
{
public:
    enum class Scope
    {
        Generic,            /**< Generic url. */
        Direct              /**< Direct-access url. */
    };

    enum class Protocol
    {
        Http,
        Https,
        Native,
    };

    enum class ClientCommand
    {
        None,
        LoginToCloud,       /**< Login to cloud. Auth is required. */
        Client,             /**< Open client and optionally login to system. */
        OpenOnPortal,       /**< Open the given system on the portal. Never native. */
    };

    enum class SystemAction
    {
        View,               /**< Open some cameras. */
    };

    /** Referral links - source part. */
    enum class ReferralSource
    {
        None,
        DesktopClient,
        MobileClient,
        CloudPortal,
        WebAdmin,
    };

    /** Referral links - context part. */
    enum class ReferralContext
    {
        None,
        SetupWizard,
        SettingsDialog,     /**< System administration dialog or other internal dialogs. */
        AboutDialog,
        WelcomePage,
        CloudMenu,          /**< Cloud context menu. */
    };

    SystemUri();
    SystemUri(const nx::utils::Url& url);
    SystemUri(const QString& uri);
    SystemUri(const SystemUri& other);
    virtual ~SystemUri();

    Scope scope() const;
    void setScope(Scope value);

    Protocol protocol() const;
    void setProtocol(Protocol value);

    QString domain() const;
    void setDomain(const QString& value);

    ClientCommand clientCommand() const;
    void setClientCommand(ClientCommand value);

    QString systemId() const;
    void setSystemId(const QString& value);

    bool hasCloudSystemId() const;

    SystemAction systemAction() const;
    void setSystemAction(SystemAction value);

    struct Auth
    {
        QString user;
        QString password;
        QString authCode;

        NX_VMS_UTILS_API QString encode() const;
    };
    const Auth& authenticator() const;
    void setAuthenticator(const Auth& value);
    void setAuthenticator(const QString& user, const QString& password);

    using ResourceIdList = QList<QnUuid>;
    void setResourceIds(const ResourceIdList& resourceIds);
    ResourceIdList resourceIds() const;

    void setTimestamp(qint64 value);
    qint64 timestamp() const;

    struct Referral
    {
        ReferralContext context = ReferralContext::None;
        ReferralSource source = ReferralSource::None;
    };
    Referral referral() const;
    void setReferral(const Referral& value);
    void setReferral(ReferralSource source, ReferralContext context);

    /** Raw parameters using is strongly discouraged. */
    typedef QHash<QString, QString> Parameters;
    Parameters rawParameters() const;
    void setRawParameters(const Parameters& value);

    void addParameter(const QString& key, const QString& value);

    bool isNull() const;

    bool isValid() const;

    QString toString() const;
    nx::utils::Url toUrl() const;

    //! QUrl for connection to a given system.
    nx::utils::Url connectionUrl() const;

    // TODO: #sivanov Use nx_reflect.
    static QString toString(SystemUri::Scope value);
    static QString toString(SystemUri::Protocol value);
    static QString toString(SystemUri::ClientCommand value);
    static QString toString(SystemUri::SystemAction value);
    static QString toString(SystemUri::ReferralSource value);
    static QString toString(SystemUri::ReferralContext value);

    SystemUri& operator=(const SystemUri& other);
    bool operator==(const SystemUri& other) const;
private:
    QScopedPointer<SystemUriPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(SystemUri)
};

} // namespace nx::vms::utils
