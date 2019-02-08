#include "web_view_dialog.h"

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>
#include <QtWebKitWidgets/QWebView>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFrame>

#include <ui/widgets/common/web_page.h>

#include <nx/vms/client/desktop/common/widgets/web_widget.h>

namespace nx::vms::client::desktop {

WebViewDialog::WebViewDialog(const QUrl& url, QWidget* parent):
    base_type(parent, Qt::Window)
{
    auto webWidget = new WebWidget(this);
    webWidget->view()->setPage(new QnWebPage(this));

    auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(QMargins());
    mainLayout->addWidget(webWidget);
    mainLayout->addWidget(line);
    mainLayout->addWidget(buttonBox);

    webWidget->load(url);
}

void WebViewDialog::showUrl(const QUrl& url, QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(url, parent));
    dialog->exec();
}

} // namespace nx::vms::client::desktop
