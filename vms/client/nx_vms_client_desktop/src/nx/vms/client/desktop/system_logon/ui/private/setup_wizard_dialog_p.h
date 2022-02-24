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

public slots:
    void openUrlInBrowser(const QString &urlString);

    QString getCredentials() const;

    void updateCredentials(const QString& login,
        const QString& password,
        bool /*isCloud*/ alwaysFalse,
        bool savePassword);

    void cancel();

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
