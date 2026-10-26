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
        ? tr("%1\nThis integration presented an untrusted certificate. It is not recommended to"
             " open this integration unless the risks are understood and accepted.",
              "%1 is the integration address")
              .arg(addressLine)
        : tr("%1\nThis web page presented an untrusted certificate. It is not recommended to"
             " open this web page unless the risks are understood and accepted.",
              "%1 is the web page address")
              .arg(addressLine);

    setInformativeText(infoText);

    addButton(isIntegration ? tr("Open Integration") : tr("Open Web Page"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Warning);

    setStandardButtons({QDialogButtonBox::Cancel});
    button(QDialogButtonBox::Cancel)->setFocus();
}

} // nx::vms::client::desktop
