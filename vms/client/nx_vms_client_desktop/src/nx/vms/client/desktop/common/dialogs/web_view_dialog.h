// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/utils/abstract_web_authenticator.h>
#include <ui/dialogs/common/button_box_dialog.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class WebViewDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    /**
     * Constructs WebViewDialog.
     * @param url Url to show.
     * @param enableClientApi Enable client api.
     * @param context Workbench context.
     * @param resource Associated resource (used while proxying).
     * @param authenticator Authenticator, which should be used during the authentication.
     * @param checkCertificate Enable certificate check.
     * @param parent Dialog's parent.
     */
    explicit WebViewDialog(
        const QUrl& url,
        bool enableClientApi = false,
        QnWorkbenchContext* context = nullptr,
        const QnResourcePtr& resource = {},
        std::shared_ptr<AbstractWebAuthenticator> authenticator = nullptr,
        bool checkCertificate = true,
        QWidget* parent = nullptr);

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
    static void showUrl(const QUrl& url,
        bool enableClientApi = false,
        QnWorkbenchContext* context = nullptr,
        const QnResourcePtr& resource = {},
        std::shared_ptr<AbstractWebAuthenticator> authenticator = nullptr,
        bool checkCertificate = true,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
