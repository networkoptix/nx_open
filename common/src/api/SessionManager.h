#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "utils/common/base.h"

class QNetworkReply;

class SyncHTTP;

namespace detail {
    class SessionManagerReplyProcessor : public QObject
    {
        Q_OBJECT

    public:
        SessionManagerReplyProcessor(QObject *parent = 0) : QObject(parent) {}

    Q_SIGNALS:
        void finished(int status, const QByteArray &result, int handle);

    private Q_SLOTS:
        void at_replyReceived(QNetworkReply *reply);
    };
}


class SessionManager : public QObject
{
    Q_OBJECT

public:
    SessionManager(const QUrl &url, QObject *parent = 0);
    virtual ~SessionManager();

    static SessionManager *instance();

    void testConnectionAsync(QObject* receiver, const char *slot);

    QByteArray lastError() const;

    void setAddEndSlash(bool value);

Q_SIGNALS:
    void error(int error);

protected Q_SLOTS:
    void setupErrorHandler();

protected:
    int sendGetRequest(const QString &objectName, QByteArray &reply);
    int sendGetRequest(const QString &objectName, const QnRequestParamList &params, QByteArray &reply);

    int sendAsyncGetRequest(const QString &objectName, QObject *target, const char *slot);
    int sendAsyncGetRequest(const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot);

    static QByteArray formatNetworkError(int error);

    QUrl createApiUrl(const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;

protected:
    SyncHTTP *m_httpClient;
    QByteArray m_lastError;
    bool m_addEndSlash;
    static QAtomicInt m_handle;
private:
    Q_DISABLE_COPY(SessionManager)
};

#endif // __SESSION_MANAGER_H__
