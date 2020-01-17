#include "web_view_dialog.h"

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>
#include <QtWebKitWidgets/QWebView>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFrame>

#include <nx/vms/client/desktop/common/widgets/web_widget.h>
#include <nx/vms/client/desktop/common/widgets/web_engine_view.h>

namespace {

static constexpr QSize kBaseDialogSize(600, 500);

} // namespace

namespace nx::vms::client::desktop {

WebViewDialog::WebViewDialog(const QUrl& url, QWidget* parent):
    base_type(parent, Qt::Window)
{
    auto webWidget = new WebWidget(this);
    auto webView = webWidget->webEngineView();
    webView->setIgnoreSslErrors(true);
    webView->setHiddenActions({
        QWebEnginePage::DownloadImageToDisk,
        QWebEnginePage::DownloadLinkToDisk,
        QWebEnginePage::DownloadMediaToDisk,
        QWebEnginePage::SavePage});

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
    setBaseSize(kBaseDialogSize);

    webWidget->load(url);
}

void WebViewDialog::showUrl(const QUrl& url, QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(url, parent));
    dialog->exec();
}

} // namespace nx::vms::client::desktop
