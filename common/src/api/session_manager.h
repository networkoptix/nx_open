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
    SyncRequestProcessor(QObject *parent = NULL): QObject(parent), m_finished(false) {}

    int wait(QnHTTPRawResponse& response);

private slots:
    void at_finished(const QnHTTPRawResponse&, int handle);
    void at_destroy();

private:
    bool m_finished;
    QnHTTPRawResponse m_response;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

class SessionManagerReplyProcessor : public QObject 
{
    Q_OBJECT

public:
    SessionManagerReplyProcessor(int handle, QObject *parent = NULL): 
        QObject(parent),
        m_handle(handle)
    {}

signals:
    void finished(const QnHTTPRawResponse&, int handle);

private slots:
    void at_replyReceived();

private:
    int m_handle;
};


class QnSessionManager: public QObject 
{
    Q_OBJECT

public:
    QnSessionManager(QObject *parent = NULL);
    virtual ~QnSessionManager();

    static QnSessionManager *instance();

    static QByteArray formatNetworkError(int error);
    static bool checkIfAppServerIsOld();

    void stop();
    void start();
    bool isReady() const;

    // Synchronous requests return status
    int sendGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response);
    int sendGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QnHTTPRawResponse& response);
    int sendPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QnHTTPRawResponse& response);
    int sendPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response);
    QByteArray lastError() const;

    // Asynchronous requests return request handle
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);

signals:
    void replyReceived(int status);

private:
    QUrl createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;

private slots:
    void doStop();
    void doStart();

    void doSendAsyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params);
    void doSendAsyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray &data);
    void doSendAsyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, int id);
    void processReply(const QnHTTPRawResponse& response, int handle);

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void asyncGetRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params);
    void asyncPostRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data);
    void asyncDeleteRequest(SessionManagerReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, int id);

private:
    QNetworkAccessManager *m_accessManager;
    mutable QMutex m_accessManagerMutex;
    static QAtomicInt s_handle;
};

#endif // __SESSION_MANAGER_H__
