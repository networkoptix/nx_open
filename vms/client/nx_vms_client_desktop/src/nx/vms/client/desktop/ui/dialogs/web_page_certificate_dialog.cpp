// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_page_certificate_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/common/html/html.h>

namespace nx::vms::client::desktop {

WebPageCertificateDialog::WebPageCertificateDialog(
    QWidget* parent,
    const QUrl& url,
    bool isIntegration)
    :
    QnSessionAwareMessageBox(parent)
{
    setText(isIntegration
        ? tr("Open this integration?")
        : tr("Open this web page?"));

    setIcon(QnMessageBox::Icon::Question);

    const QString addressLine = isIntegration
        ? nx::vms::common::html::bold(tr("Integration")).append(": ").append(url.toDisplayString())
        : nx::vms::common::html::bold(tr("Web Page")).append(": ").append(url.toDisplayString());

    const QString infoText = isIntegration
        ? tr("You try to open the\n%1\nbut this integration presented an untrusted certificate"
            " auth.\nWe recommend you not to open this integration. If you understand the risks,"
            " you can open the integration.", "%1 is the integration address").arg(addressLine)
        : tr("You try to open the\n%1\nbut this web page presented an untrusted certificate auth."
            "\nWe recommend you not to open this web page. If you understand the risks, you can"
            " open the web page.", "%1 is the web page address").arg(addressLine);

    setInformativeText(infoText);

    addButton(tr("Connect anyway"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    setStandardButtons({QDialogButtonBox::Cancel});
    button(QDialogButtonBox::Cancel)->setFocus();
}

} // nx::vms::client::desktop
