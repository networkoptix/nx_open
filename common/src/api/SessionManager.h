#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include "utils/network/simple_http_client.h"

class SessionManager
{
public:
    SessionManager(const QHostAddress& host, int port, const QAuthenticator& auth);
    virtual ~SessionManager();

    void setAddEndShash(bool value);
protected:
    int sendGetRequest(QString objectName, QByteArray& reply);
    int sendGetRequest(QString objectName, QnRequestParamList, QByteArray& reply);

protected:
    CLSimpleHTTPClient m_client;
    bool m_addEndShash;
};

#endif // __SESSION_MANAGER_H__
