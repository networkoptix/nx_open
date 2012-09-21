#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include "utils/common/request_param.h"
#include "utils/common/long_runnable.h"


class QNetworkReply;

class SyncRequestProcessor : public QObject
{
    Q_OBJECT

public:
    SyncRequestProcessor(QObject *parent = 0)
        : QObject(parent),
          m_finished(false)
    {
    }

private slots:
    void at_finished(int status, const QByteArray &data, const QByteArray &errorString, int handle);
    void at_destroy();

public:
    int wait(QByteArray &reply, QByteArray &errorString);

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
    SessionManagerReplyProcessor(QObject *parent, QObject* target, const char* slot, int handle)
        : QObject(parent),
          m_target(target),
          m_slot(slot),
          m_handle(handle)
    {
    }

signals:
    void finished(int status, const QByteArray& data, const QByteArray& errorString, int handle);

private slots:
    void at_replyReceived();

private:
    QObject *m_target;
    const char *m_slot;
    int m_handle;
};


class QnSessionManager : public QObject
{
    Q_OBJECT

public:
    static QnSessionManager *instance();

    QByteArray lastError() const;

    static QByteArray formatNetworkError(int error);

    bool isReady() const;

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void replyReceived(int status);

private slots:
    void doStop();
    void doStart();

    void doSendAsyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle);
    void doSendAsyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, const QnRequestParamList &params, const QByteArray &data, QObject *target, const char *slot, int handle);
    void doSendAsyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, int id, QObject *target, const char *slot, int handle);

    void processReply(int status, const QByteArray &data, const QByteArray &errorString, int handle);

public:
    QnSessionManager();
    virtual ~QnSessionManager();

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
    int sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot);

signals:
    void asyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, QObject *target, const char *slot, int handle);
    void asyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, int handle);
    void asyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, int handle);

private:
    QNetworkAccessManager* m_accessManager;
    mutable QMutex m_accessManagerMutex;

    static QAtomicInt m_handle;

private:
    QUrl createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;

private:
    Q_DISABLE_COPY(QnSessionManager)
};

#endif // __SESSION_MANAGER_H__
