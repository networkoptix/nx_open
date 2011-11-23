#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QObject>
#include "utils/network/synchttp.h"
#include "utils/common/base.h"

class QNetworkReply;

namespace detail {
    class SessionManagerReplyProcessor: public QObject {
        Q_OBJECT;
    public:
        SessionManagerReplyProcessor(QObject *parent = NULL): QObject(parent) {}

    private slots:
        void at_replyReceived(QNetworkReply *reply);

    signals:
        void finished(int status, const QByteArray &result);
    };
}

class SessionManager: public QObject {
    Q_OBJECT;
public:
    SessionManager(const QHostAddress& host, quint16 port, const QAuthenticator& auth, QObject *parent = NULL);
    virtual ~SessionManager();

    QByteArray getLastError();

    void setAddEndShash(bool value);

protected:
    int sendGetRequest(const QString &objectName, QByteArray &reply);
    int sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray &reply);

    void sendAsyncGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot);
    void sendAsyncGetRequest(const QString &objectName, QObject *target, const char *slot);

    QByteArray formatNetworkError(int error);

    QUrl createApiUrl(const QString &objectName, const QnRequestParamList &params);

protected:
    SyncHTTP m_httpClient;
    QByteArray m_lastError;
    bool m_addEndShash;
};

#endif // __SESSION_MANAGER_H__
