#include "web_page.h"

#include <nx/utils/log/log.h>

QnWebPage::QnWebPage(QObject* parent /*= 0*/)
    : base_type(parent)
{

}

void QnWebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
{
    QString logMessage = lit("JS Console: %1:%2 %3").arg(sourceID, QString::number(lineNumber), message);
    for (const QString& part : logMessage.split(lit("\n")))
        qDebug() << part;
    NX_LOG(logMessage, cl_logINFO);
}
