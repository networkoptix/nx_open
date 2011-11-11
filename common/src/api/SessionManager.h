#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include "utils/network/synchttp.h"
#include "utils/common/base.h"

class SessionManager
{
public:
    SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth);
    virtual ~SessionManager();

    void setAddEndShash(bool value);
protected:
    int sendGetRequest(QString objectName, QByteArray& reply);
    int sendGetRequest(QString objectName, QnRequestParamList, QByteArray& reply);

protected:
    SyncHTTP m_httpClient;
    // CLSimpleHTTPClient m_client;
    bool m_addEndShash;
};

#endif // __SESSION_MANAGER_H__
