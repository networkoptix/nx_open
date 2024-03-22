// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_view_dialog.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>

#include <nx/network/ssl/certificate.h>
#include <nx/vms/client/desktop/common/widgets/web_widget.h>
#include <nx/vms/client/desktop/ui/dialogs/web_page_certificate_dialog.h>
#include <ui/workbench/workbench_context.h>

namespace {

static constexpr QSize kBaseDialogSize(600, 500);

} // namespace

namespace nx::vms::client::desktop {

WebViewDialog::WebViewDialog(
    const QUrl& url,
    bool enableClientApi,
    QnWorkbenchContext* context,
    const QnResourcePtr& resource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    bool checkCertificate,
    QWidget* parent)
    :
    base_type(parent, Qt::Window)
{
    auto webWidget = new WebWidget(this);

    auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(QMargins());
    mainLayout->addWidget(webWidget);
    mainLayout->addWidget(line);
    mainLayout->addWidget(buttonBox);

    // Set some resonable size to avoid completely shrinked dialog.
    resize(kBaseDialogSize);

    if (checkCertificate)
    {
        webWidget->controller()->setCertificateValidator(
            [context](const QString& certificateChain, const QUrl& url)
            {
                if (certificateChain.isEmpty())
                    return false;

                const auto chain =
                    nx::network::ssl::Certificate::parse(certificateChain.toStdString());

                if (chain.empty())
                    return false;

                return WebPageCertificateDialog(context->mainWindowWidget(), url).exec()
                    == QDialogButtonBox::AcceptRole;
            });
    }
    else
    {
        webWidget->controller()->setCertificateValidator(
            [](const QString& /*certificateChain*/, const QUrl&) { return true; });
    }

    webWidget->controller()->setAuthenticator(authenticator);
    webWidget->controller()->load(resource, url);

    if (enableClientApi && context)
    {
        auto authCondition = [](const QUrl&) { return true; };
        webWidget->controller()->initClientApiSupport(context, /*item*/ nullptr, authCondition);
    }
}

void WebViewDialog::showUrl(
    const QUrl& url,
    bool enableClientApi,
    QnWorkbenchContext* context,
    const QnResourcePtr& resource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    bool checkCertificate,
    QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(
        url, enableClientApi, context, resource, authenticator, checkCertificate, parent));

    dialog->exec();
}

} // namespace nx::vms::client::desktop
