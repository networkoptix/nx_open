#include "web_page.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <nx/utils/log/log.h>

namespace {

void trace(const QString& message)
{
#ifdef _DEBUG
    for (const QString& line : message.split(lit("\n"), QString::SkipEmptyParts))
        qDebug() << line;
#endif
    NX_LOG(message, cl_logINFO);
}

}

QnWebPage::QnWebPage(QObject* parent):
    base_type(parent)
{
    connect(this->networkAccessManager(), &QNetworkAccessManager::sslErrors, this,
        [](QNetworkReply* reply, const QList<QSslError>& errors)
        {
            for (auto e: errors)
                trace(e.errorString());
            reply->ignoreSslErrors();
        });
}

void QnWebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
{
    QString logMessage = lit("JS Console: %1:%2 %3").arg(sourceID, QString::number(lineNumber), message);
    trace(logMessage);
}
