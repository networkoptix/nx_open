#include "AppSessionManager.h"

#include <QtNetwork/QNetworkReply>

#include <QtXml/QDomDocument>
#include <QtXml/QXmlInputSource>

#include "utils/network/synchttp.h"

AppSessionManager::AppSessionManager(const QUrl &url)
    : SessionManager(url)
{
}

int AppSessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& reply, QByteArray& errorString)
{
    QTextStream stream(&reply);

    int status = m_httpClient->syncPost(createApiUrl(objectName), body, stream.device());
    stream.readAll();

    if (status != 0)
    {
        errorString += "\nAppSessionManager::addObject(): ";
        errorString += formatNetworkError(status) + reply;
    }

    return status;
}

int AppSessionManager::addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot)
{
    return sendAsyncPostRequest(objectName, data, target, slot);
}

int AppSessionManager::getObjects(const QString& objectName, const QString& args, QByteArray& data,
               QByteArray& errorString)
{
    QString request;

    if (args.isEmpty())
        request = objectName;
    else
        request = QString("%1/%2").arg(objectName).arg(args);

    return sendGetRequest(request, data, errorString);
}
