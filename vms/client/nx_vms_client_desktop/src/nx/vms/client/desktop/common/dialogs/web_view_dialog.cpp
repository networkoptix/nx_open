// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_view_dialog.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/common/widgets/web_widget.h>

namespace {

static constexpr QSize kBaseDialogSize(600, 500);

} // namespace

namespace nx::vms::client::desktop {

WebViewDialog::WebViewDialog(
    const QUrl& url,
    bool enableClientApi,
    QnWorkbenchContext* context,
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

    webWidget->controller()->load(url);

    if (enableClientApi && context)
    {
        auto authCondition = [] { return true; };
        webWidget->controller()->initClientApiSupport(context, /*item*/ nullptr, authCondition);
    }
}

void WebViewDialog::showUrl(
    const QUrl& url,
    bool enableClientApi,
    QnWorkbenchContext* context,
    QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(url, enableClientApi, context, parent));
    dialog->exec();
}

} // namespace nx::vms::client::desktop
