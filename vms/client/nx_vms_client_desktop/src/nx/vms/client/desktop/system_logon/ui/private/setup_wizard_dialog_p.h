// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/network/socket_common.h>

namespace nx::vms::client::desktop {

class SetupWizardDialog;
class WebViewWidget;

class SetupWizardDialogPrivate: public QObject
{
    Q_OBJECT
    SetupWizardDialog* const q;

public:
    SetupWizardDialogPrivate(SetupWizardDialog* parent);

// Public API to be used in the web page.
public slots:
    /**
     * Open external url in the browser.
     */
    void openUrlInBrowser(const QString& urlString);

    /**
     * Cloud refresh token if user is logged in to cloud, empty string otherwise.
     */
    QString refreshToken() const;

    /**
     * Connect to the System using local administrator with the provided password.
     */
    void connectUsingLocalAdmin(const QString& password);

    /**
     * Connect to the System using Cloud account.
     */
    void connectUsingCloud();

    /**
     * Json object with the LoginInfo structure.
     * Method is deprecated and will be removed as soon as it is not needed.
     */
    QString getCredentials() const;

    /**
     * Fill LoginInfo structure with the provided values. Third parameter is ignored.
     * Method is deprecated and will be removed in favor of `connect...` methods.
     */
    void updateCredentials(
        const QString& login,
        const QString& password,
        bool /*isCloud*/ alwaysFalse,
        bool savePassword);

    /**
     * Close dialog.
     */
    void cancel();

    /**
     * Open url in the dialog.
     */
    void load(const QUrl& url);

public:
    struct LoginInfo
    {
        QString localLogin;
        QString localPassword;
        bool savePassword = false;
    };

    nx::vms::client::desktop::WebViewWidget* webViewWidget;
    nx::network::SocketAddress address;
    LoginInfo loginInfo;
};

} // namespace nx::vms::client::desktop
