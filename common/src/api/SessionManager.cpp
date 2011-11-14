#include <QTextStream>

#include "SessionManager.h"

SessionManager::SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth)
    : m_httpClient(host, port, auth)
    // : m_client(host, port, 10000, auth)
{
    m_addEndShash = true;
}

SessionManager::~SessionManager()
{

}

int SessionManager::sendGetRequest(QString objectName, QByteArray& reply)
{
    return sendGetRequest(objectName, QnRequestParamList(), reply);
}

int SessionManager::sendGetRequest(QString objectName, QnRequestParamList params, QByteArray& reply)
{
    QUrl url;
    url.setPath(objectName);
    if (m_addEndShash)
        url.setPath(url.path() +  "/");
    for (int i = 0; i < params.size(); ++i)
    {
        url.addQueryItem(params[i].first, params[i].second);
    }
    QString dd = url.toString();

    QTextStream stream(&reply);

    return m_httpClient.syncGet(QByteArray("api/") + url.toEncoded(), stream.device());
    // CLHttpStatus status = m_client.doGET(QByteArray("api/") + url.toEncoded());

//    if (status == CL_HTTP_SUCCESS || status == CL_HTTP_BAD_REQUEST)
//        m_client.readAll(reply);
//            reply = stream.readAll();
    
    // return 0; // status == CL_HTTP_SUCCESS ? 0 : 1;
}

void SessionManager::setAddEndShash(bool value)
{
    m_addEndShash = value;
}
