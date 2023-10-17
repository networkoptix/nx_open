// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/utils/abstract_web_authenticator.h>
#include <ui/dialogs/common/button_box_dialog.h>

namespace nx::vms::client::desktop {

class WindowContext;

class WebWidget;

class WebViewDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    WebViewDialog(QWidget* parent = nullptr);
    virtual ~WebViewDialog() override = default;

    /**
     * Shows Webpage.
     * @param url Url to show.
     * @param enableClientApi Enable client api.
     * @param context Workbench context.
     * @param resource Associated resource (used while proxying).
     * @param authenticator Authenticator, which should be used during the authentication.
     * @param checkCertificate Enable certificate check.
     * @param parent Dialog's parent.
     */
    int showUrl(
        const QUrl& url,
        bool enableClientApi,
        WindowContext* context,
        const QnResourcePtr& resource = {},
        std::shared_ptr<AbstractWebAuthenticator> authenticator = nullptr,
        bool checkCertificate = true);

private:
    QPointer<WebWidget> m_webWidget;
};

} // namespace nx::vms::client::desktop
