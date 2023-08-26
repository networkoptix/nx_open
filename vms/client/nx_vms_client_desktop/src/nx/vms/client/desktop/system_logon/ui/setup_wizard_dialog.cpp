// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "setup_wizard_dialog.h"

#include <QtCore/QUrlQuery>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLineEdit>

#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <nx/network/http/http_types.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>

#include "private/setup_wizard_dialog_p.h"

namespace nx::vms::client::desktop {

namespace {
static const QSize kSetupWizardSize(520, 456);

QUrl constructUrl(const nx::network::SocketAddress& address)
{
    QUrlQuery q;
    q.addQueryItem("lang", qnRuntime->locale());

    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .setPath("/static/inline.html")
        .setFragment("/setup")
        .setQuery(q)
        .toUrl()
        .toQUrl();
}

} // namespace

SetupWizardDialog::SetupWizardDialog(
    nx::network::SocketAddress address,
    const QnUuid& serverId,
    QWidget* parent)
    :
    base_type(parent, Qt::MSWindowsFixedSizeDialogHint),
    d(new SetupWizardDialogPrivate(this)),
    m_address(std::move(address))
{
    d->webViewWidget->controller()->setCertificateValidator(
        [serverId](const QString& certificateChain, const QUrl& url)
        {
            // We accept expired certificate on the first connection to a system.
            return qnClientCoreModule->networkModule()->certificateVerifier()->verifyCertificate(
                serverId,
                nx::network::ssl::Certificate::parse(certificateChain.toStdString()),
                /*acceptExpired*/ true
            ) == nx::vms::client::core::CertificateVerifier::Status::ok;
        });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());

    if (ini().developerMode)
    {
        auto urlLineEdit = new QLineEdit(this);
        layout->addWidget(urlLineEdit);
        connect(urlLineEdit, &QLineEdit::returnPressed, this,
            [this, urlLineEdit]()
            {
                d->load(urlLineEdit->text());
            });
    }

    layout->addWidget(d->webViewWidget);
    setFixedSize(kSetupWizardSize);
    setHelpTopic(this, HelpTopic::Id::Setup_Wizard);
}

SetupWizardDialog::~SetupWizardDialog()
{
}

int SetupWizardDialog::exec()
{
    loadPage();
    return base_type::exec();
}

void SetupWizardDialog::loadPage()
{
    const auto url = constructUrl(m_address);

    if (ini().developerMode)
    {
        if (const auto lineEdit = findChild<QLineEdit*>())
            lineEdit->setText(url.toString());
    }

    NX_DEBUG(this, nx::format("Opening setup URL: %1").arg(url.toString(QUrl::RemovePassword)));

    d->load(url);
}

QString SetupWizardDialog::password() const
{
    return d->localPassword;
}

} // namespace nx::vms::client::desktop
