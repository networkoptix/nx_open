#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QNetworkAccessManager>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include "utils/common/base.h"
#include "utils/common/longrunnable.h"


class QNetworkReply;

class SyncRequestProcessor : public QObject
{
    Q_OBJECT

public:
    SyncRequestProcessor(QObject* parent = 0)
        : QObject(parent),
          m_finished(false)
    {
    }

private slots:
    void finished(int status, const QByteArray& data, int handle);
    void at_destroy();

public:
    int wait(QByteArray& reply, QByteArray& errorString);

private:
    bool m_finished;

    int m_status;
    QByteArray m_reply;
    QByteArray m_errorString;

    QMutex m_mutex;
    QWaitCondition m_condition;
};

class SessionManagerReplyProcessor : public QObject
{
    Q_OBJECT

public:
    SessionManagerReplyProcessor(QObject *parent = 0) : QObject(parent) {}

signals:
    void finished(int status, const QByteArray& data, int handle);

private slots:
    void at_replyReceived(QNetworkReply *reply);
};


class SessionManager : public QObject
{
    Q_OBJECT

public:
    static SessionManager *instance();

    QByteArray lastError() const;

    static QByteArray formatNetworkError(int error);

Q_SIGNALS:
    void error(int error);

private:
    static SessionManager* m_instance;

private slots:
    void doStop();
    void doStart();

    void doSendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle);
    void doSendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, int handle);

public:
    SessionManager();
    virtual ~SessionManager();

    void stop();
    void start();

    // Synchronous requests return status
    int sendGetRequest(const QUrl& url, const QString &objectName, QByteArray &reply, QByteArray& errorString);
    int sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QByteArray &reply, QByteArray& errorString);

    int sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QByteArray &reply, QByteArray& errorString);
    int sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QByteArray &reply, QByteArray& errorString);

    // Asynchronous requests return request handle
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot);

    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot);

    int testConnectionAsync(const QUrl& url, QObject* receiver, const char *slot);

signals:
    void stopSignal();
    void startSignal();

    void asyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle);
    void asyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, int handle);

private:
    QNetworkAccessManager* m_accessManager;
    SessionManagerReplyProcessor m_replyProcessor;
    static QAtomicInt m_handle;

private:
    QUrl createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;

private:
    Q_DISABLE_COPY(SessionManager)
};

#endif // __SESSION_MANAGER_H__
