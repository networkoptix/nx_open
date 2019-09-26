#include "web_page.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <utils/web_downloader.h>
#include <utils/common/delayed.h>

namespace {

const char * stubPage = R"HTML(
<html>
<style>
html {
    height: 100%;
    width: 100%;
}

body {
    display: table;
    height: 100%;
    margin: 0;
    padding: 0;
    width: 100%;
    background-color: {background-color};

    font-family: 'Roboto-Regular', 'Roboto';
    font-weight: 400;
    font-style: normal;
    font-kerning: normal;
}

#placeholder
{
    height: 120px; 
}

img {
    height: 100%;
    opacity: 0.7;
}

#label1 {
    width: 350px;
    font-size: 28px;
    line-height: 40px;
    margin-top: 8px;
    color: {font1-color};
    opacity: 0.7;
}

#label2 {
    width: 80%;
    word-break: break-all;
    height: 40px;
    font-size: 11px;
    line-height: 20px;
    color: {font2-color};
}

#row1 {
    display: table-row;
    height: 100%;
}

#row2 {
  display: table-row;
  {row2-display}
}

.container {
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    height: 100%;
    margin-top: -24px;
}

.box {
    width: 100%;
    text-align: center;
}

</style>
<body>

<div id="row1">
    <div class="container">
        <div class="box" id="placeholder">
            <img {image-source}>
        </div>
        <div class="box" id="label1">
            {label1-text}
        </div>
        </div>
    </div>

    <div id="row2">
        <div class="container">
            <div class="box" id="label2">
                {label2-text}
            </div>
        </div>
    </div>

</body>
</html>
)HTML";

QString nameFromUrl(const QUrl& url)
{
    const QString urlStr = url.toString();
    return urlStr.mid(urlStr.lastIndexOf('/') + 1);
}

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

QnWebPage::QnWebPage(QObject* parent):
    base_type(parent),
    // Network access manager should not be deallocated before QWebPage destructor,
    // so we use custom deleter which calls deleteLater.
    // Unfortunately custom deleter can not be passed to make_shared and we loose
    // exception safety here.
    // One way to overcome this is by using allocate_shared with simple allocator,
    // but it seems like an overkill.
    m_networkAccessManager(
        new Http1NetworkAccessManager(), [](QObject* p) { p->deleteLater(); })
{
    setNetworkAccessManager(m_networkAccessManager.get());

    connect(networkAccessManager(), &QNetworkAccessManager::sslErrors, this,
        [this](QNetworkReply* reply, const QList<QSslError>& errors)
        {
            for (auto e: errors)
                NX_DEBUG(this, "SSL error: %1 (%2)", e, int(e.error()));

            reply->ignoreSslErrors();
        });

    setForwardUnsupportedContent(true);
    connect(this, &QWebPage::downloadRequested, this,
        [this](const QNetworkRequest& request)
        {
            download(m_networkAccessManager->get(request));
        });

    connect(this, &QWebPage::unsupportedContent, this,
        [this](QNetworkReply* reply)
        {
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

    QString errPage =
        QString(stubPage)
            .replace("{font1-color}",
                nx::vms::client::desktop::colorTheme()->color(showUrl ? "light16" : "red_core")
                    .name(QColor::HexRgb))
            .replace("{font2-color}",
                nx::vms::client::desktop::colorTheme()->color("light10").name(QColor::HexRgb))
            .replace("{background-color}",
                nx::vms::client::desktop::colorTheme()->color("dark6").name(QColor::HexRgb))
            .replace("{image-source}",
                showUrl ? QString("src=\"%1\"").arg("qrc:/skin/item_placeholders/downloading.svg")
                        : QString("srcset=\"%1.png 1x, %1@2x.png 2x\"")
                              .arg("qrc:/skin/item_placeholders/no_signal"))
            .replace("{label1-text}",
                showUrl ? tr("DOWNLOADING<br>STARTED") : errOption->errorString.toUpper())
            .replace("{label2-text}", showUrl ? nameFromUrl(errOption->url) : QString())
            .replace("{row2-display}", showUrl ? "" : "display: none;");

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
