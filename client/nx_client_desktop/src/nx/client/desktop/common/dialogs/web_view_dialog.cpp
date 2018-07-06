#include "web_view_dialog.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QWebView>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>

#include <ui/style/webview_style.h>
#include <ui/widgets/common/web_page.h>
#include <nx/client/desktop/common/widgets/busy_indicator.h>
#include <nx/client/desktop/common/utils/widget_anchor.h>

namespace nx {
namespace client {
namespace desktop {

WebViewDialog::WebViewDialog(QWidget* parent):
    base_type(parent, Qt::Window),
    m_webView(new QWebView(this)),
    m_indicator(new BusyIndicatorWidget(m_webView)),
    m_errorLabel(new QLabel(m_webView))
{
    new WidgetAnchor(m_indicator);
    new WidgetAnchor(m_errorLabel);

    m_errorLabel->hide();
    m_errorLabel->setText(lit("<h1>%1</h1>").arg(tr("Failed to load page")));
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setForegroundRole(QPalette::WindowText);

    auto line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(QMargins());
    mainLayout->addWidget(m_webView);
    mainLayout->addWidget(line);
    mainLayout->addWidget(buttonBox);

    m_webView->setPage(new QnWebPage(this));

    m_webView->settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    m_webView->settings()->setAttribute(QWebSettings::Accelerated2dCanvasEnabled, true);
    m_webView->settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);

    NxUi::setupWebViewStyle(m_webView);

    static constexpr int kRadius = 8;
    m_indicator->dots()->setDotRadius(kRadius);
    m_indicator->dots()->setDotSpacing(kRadius * 2);

    const auto handleLoadStarted =
        [this]()
        {
            m_errorLabel->hide();
            m_indicator->show();
        };

    const auto handleLoadFinished =
        [this](bool success)
        {
            m_errorLabel->setHidden(success);
            m_indicator->hide();
        };

    connect(m_webView, &QWebView::loadStarted, this, handleLoadStarted);
    connect(m_webView, &QWebView::loadFinished, this, handleLoadFinished);
    connect(m_webView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError>&) { reply->ignoreSslErrors(); });
}

WebViewDialog::WebViewDialog(const QString& url, QWidget* parent):
    WebViewDialog(QUrl::fromUserInput(url), parent)
{
}

WebViewDialog::WebViewDialog(const QUrl& url, QWidget* parent):
    WebViewDialog(parent)
{
    m_webView->load(url);
}

void WebViewDialog::showUrl(const QString& url, QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(url, parent));
    dialog->exec();
}

void WebViewDialog::showUrl(const QUrl& url, QWidget* parent)
{
    QScopedPointer<WebViewDialog> dialog(new WebViewDialog(url, parent));
    dialog->exec();
}

} // namespace desktop
} // namespace client
} // namespace nx
