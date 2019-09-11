#include "web_page.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <utils/web_downloader.h>
#include <utils/common/delayed.h>

namespace {

using WebDownloader = nx::vms::client::desktop::utils::WebDownloader;

/**
 * Explicitly forbid HTTP2 connections.
 */
class Http1NetworkAccessManager: public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
        QIODevice* outgoingData = nullptr)
    {
        QNetworkRequest copy(request);
        copy.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, false);
        return QNetworkAccessManager::createRequest(op, copy, outgoingData);
    }
};

} // namespace

QnWebPage::QnWebPage(QObject* parent): base_type(parent)
{
    m_networkAccessManager = std::make_shared<Http1NetworkAccessManager>();
    setNetworkAccessManager(m_networkAccessManager.get());

    connect(networkAccessManager(), &QNetworkAccessManager::sslErrors, this,
        [this](QNetworkReply* reply, const QList<QSslError>& errors)
        {
            for (auto e: errors)
                NX_DEBUG(this, "SSL error: %1 (%2)", e, int(e.error()));

            reply->ignoreSslErrors();
        });

    setForwardUnsupportedContent(true);
    connect(this, &QWebPage::downloadRequested, this, [this](const QNetworkRequest& request) {
        download(m_networkAccessManager->get(request));
    });

    connect(this, &QWebPage::unsupportedContent, this, [this](QNetworkReply* reply) {
        download(reply);
    });
}

void QnWebPage::download(QNetworkReply* reply)
{
    // Avoid reply deletion in event loop.
    reply->setParent(this);

    executeLater(
        [this, reply] {
            // Successful call to download() will re-parent the reply.
            if (!WebDownloader::download(m_networkAccessManager, reply, this))
                reply->deleteLater();
        },
        this);
}

bool QnWebPage::supportsExtension(Extension extension) const
{
    if (extension == QWebPage::ErrorPageExtension)
        return true;
    return base_type::supportsExtension(extension);
}

bool QnWebPage::extension(Extension extension, const ExtensionOption * option, ExtensionReturn * output)
{
    if (!option || !output)
        return base_type::extension(extension, option, output);
    if (extension != QWebPage::ErrorPageExtension)
        return base_type::extension(extension, option, output);

    const auto kLoadingIsHandledByTheMediaEngine = 203;
    const auto kFrameLoadInterruptedByPolicyChange = 102;

    const ErrorPageExtensionOption *errOption =
           static_cast<const ErrorPageExtensionOption*>(option);

    const bool requestDownload = errOption->domain == QWebPage::WebKit
        && errOption->error == kLoadingIsHandledByTheMediaEngine;
    const bool unsupportedContentError = errOption->domain == QWebPage::WebKit
        && errOption->error == kFrameLoadInterruptedByPolicyChange;
    const bool showUrl = requestDownload || unsupportedContentError;
    qDebug() << errOption->errorString;
    QString errPage;
    errPage = QString("<html><body style=\"color: %1; background-color: %2\">%3</body></html>")
        .arg(nx::vms::client::desktop::colorTheme()->color("light16").name(QColor::HexRgb))
        .arg(nx::vms::client::desktop::colorTheme()->color("dark6").name(QColor::HexRgb))
        .arg(showUrl ? errOption->url.toString() : errOption->errorString);

    if (requestDownload)
        download(m_networkAccessManager->get(QNetworkRequest(errOption->url)));

    ErrorPageExtensionReturn *errReturn =
                                static_cast<ErrorPageExtensionReturn*>(output);
    errReturn->baseUrl = errOption->url;
    errReturn->content = errPage.toUtf8();
    return true;
}

void QnWebPage::javaScriptConsoleMessage(
    const QString& message, int lineNumber, const QString& sourceID)
{
    NX_DEBUG(this, "JS Console: %1:%2 %3", sourceID, lineNumber, message);
}
