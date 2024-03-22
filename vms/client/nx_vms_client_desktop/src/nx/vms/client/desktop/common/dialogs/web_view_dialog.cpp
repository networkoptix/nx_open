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
#include <nx/vms/client/desktop/window_context.h>

namespace {

static constexpr QSize kBaseDialogSize(600, 500);

} // namespace

namespace nx::vms::client::desktop {

WebViewDialog::WebViewDialog(
    QWidget* parent,
    QDialogButtonBox::StandardButtons buttons)
    :
    base_type(parent, Qt::Window),
    m_webWidget(new WebWidget(this))
{
    auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);

    auto buttonBox = new QDialogButtonBox(buttons, this);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(QMargins());
    mainLayout->addWidget(m_webWidget);
    mainLayout->addWidget(line);
    mainLayout->addWidget(buttonBox);

    buttonBox->setVisible(buttons != QDialogButtonBox::NoButton);

    // Set some resonable size to avoid completely shrinked dialog.
    resize(kBaseDialogSize);
}

void WebViewDialog::init(
    const QUrl& url,
    bool enableClientApi,
    WindowContext* context,
    const QnResourcePtr& resource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    bool checkCertificate)
{
    if (checkCertificate)
    {
        m_webWidget->controller()->setCertificateValidator(
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
        m_webWidget->controller()->setCertificateValidator(
            [](const QString& /*certificateChain*/, const QUrl&) { return true; });
    }

    m_webWidget->controller()->setAuthenticator(authenticator);
    m_webWidget->controller()->load(resource, url);

    if (enableClientApi && context)
    {
        auto authCondition = [](const QUrl&) { return true; };
        m_webWidget->controller()->initClientApiSupport(context, authCondition);
    }

    return;
}

} // namespace nx::vms::client::desktop
