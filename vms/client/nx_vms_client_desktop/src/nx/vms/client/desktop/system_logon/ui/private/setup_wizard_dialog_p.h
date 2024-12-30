// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

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
     * Connect to the System using local administrator with the provided password.
     */
    void connectUsingLocalAdmin(const QString& password, bool /*dummy*/);

    /**
     * Binds System to Cloud.
     */
    void bindToCloud();

    /**
     * Close dialog.
     */
    void cancel();

    /**
     * Open url in the dialog.
     */
    void load(const QUrl& url);

public:
    nx::vms::client::desktop::WebViewWidget* webViewWidget;
    QString localPassword;
    bool isBindingToCloudRequested = false;
};

} // namespace nx::vms::client::desktop
