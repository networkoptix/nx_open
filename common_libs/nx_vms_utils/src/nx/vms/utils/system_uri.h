#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QHash>
#include <QtCore/QString>

#include <nx/utils/url.h>

namespace nx {
namespace vms {
namespace utils {

/**
 * Classes for handling and constructing generic URI protocol links.
 *
 * Full scheme looks like:
 * * {protocol}://{domain}/{client_command}/{system_id}{system_action}?auth={access_key}&{action_parameters}
 *
 * @see https://networkoptix.atlassian.net/wiki/display/PM/NX+URL+Scheme
 *
 * Protocol part is customizable, so we don't parse it here.
 * Auth is encoded as base64(login:pass).
 * All query parameters are url-encoded.
 */
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

    SystemAction systemAction() const;
    void setSystemAction(SystemAction value);

    struct Auth
    {
        QString user;
        QString password;

        NX_VMS_UTILS_API QString encode() const;
    };
    Auth authenticator() const;
    void setAuthenticator(const Auth& value);
    void setAuthenticator(const QString& user, const QString& password);

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

    // TODO: #GDM when fusion will be moved out to separate library, change to QnLexical
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

} // namespace utils
} // namespace vms
} // namespace nx
