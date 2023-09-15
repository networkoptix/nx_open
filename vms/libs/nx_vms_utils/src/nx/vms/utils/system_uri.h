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

#include <QtCore/QList>
#include <QtCore/QString>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

namespace nx::vms::utils {

struct NX_VMS_UTILS_API SystemUri
{
public:
    enum class Scope
    {
        /** Generic url. */
        generic,

        /** Direct access url for the Web-Admin. */
        direct
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

    /** If the link requires authorization then we need to know the type of authorization. */
    enum class UserAuthType
    {
        none,
        cloud,
        local,
        temporary,
    };
    
    struct Referral
    {
        ReferralSource source = ReferralSource::None;
        ReferralContext context = ReferralContext::None;
        bool operator==(const Referral& other) const = default;
    };

    Scope scope = Scope::generic;
    Protocol protocol = Protocol::Http;
    UserAuthType userAuthType = UserAuthType::none;

    /** Cloud host for Generic-scope urls. */
    QString cloudHost;

    ClientCommand clientCommand = ClientCommand::None;

    /** Address of the System, either if form of hostname:port or guid for Cloud Systems. */
    QString systemAddress;

    SystemAction systemAction = SystemAction::View;
    Referral referral;
    QList<QnUuid> resourceIds;
    qint64 timestamp = -1;
    nx::network::http::Credentials credentials;

    /** Authentication code for Cloud login. */
    QString authCode;

    SystemUri() = default;
    SystemUri(const SystemUri& other) = default;
    SystemUri(const nx::utils::Url& url);
    SystemUri(const QString& uri);

    bool hasCloudSystemAddress() const;

    bool isValid() const;

    /** Bearer token or encoded password credentials. */
    QString authKey() const;

    QString toString() const;
    nx::utils::Url toUrl() const;

    // TODO: #sivanov Use nx_reflect.
    static QString toString(Scope value);
    static QString toString(Protocol value);
    static QString toString(ClientCommand value);
    static QString toString(SystemAction value);
    static QString toString(ReferralSource value);
    static QString toString(ReferralContext value);
    static SystemUri fromTemporaryUserLink(QString link);

    bool operator==(const SystemUri& other) const = default;
};

} // namespace nx::vms::utils
