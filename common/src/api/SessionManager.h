#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QObject>
#include "utils/network/synchttp.h"
#include "utils/common/base.h"

namespace detail {
    class SessionManagerReplyForwarder: public QObject {
        Q_OBJECT;
    public:
        SessionManagerReplyForwarder(QObject *parent = NULL): QObject(parent) {}

    private slots:
        void at_replyReceived(QNetworkReply *reply);

    signals:
        void finished(int status, const QByteArray &result);
    };
}

class SessionManager
{
public:
    SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth);
    virtual ~SessionManager();

    QByteArray getLastError();

    void setAddEndShash(bool value);

protected:
    int sendGetRequest(const QString &objectName, QByteArray &reply);
    int sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray &reply);

    void sendGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot);

    QByteArray formatNetworkError(int error);

protected:
    SyncHTTP m_httpClient;
    QByteArray m_lastError;

    bool m_addEndShash;
};

#endif // __SESSION_MANAGER_H__
