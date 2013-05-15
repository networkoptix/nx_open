#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include "utils/common/request_param.h"

class QNetworkReply;
class QNetworkAccessManager;

class QnSessionManagerSyncReplyProcessor: public QObject {
    Q_OBJECT

public:
    QnSessionManagerSyncReplyProcessor(QObject *parent = NULL): QObject(parent), m_finished(false) {}

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


class QnSessionManagerAsyncReplyProcessor: public QObject {
    Q_OBJECT

public:
    QnSessionManagerAsyncReplyProcessor(int handle, QObject *parent = NULL): 
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


// TODO: #Elric separate into two objects, one object per thread.
class QnSessionManager: public QObject {
    Q_OBJECT

public:
    QnSessionManager(QObject *parent = NULL);
    virtual ~QnSessionManager();

    static QnSessionManager *instance();
    static QByteArray formatNetworkError(int error);

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
    int sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);

signals:
    void replyReceived(int status);

private:
    QUrl createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;
    int sendSyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response);
    int sendAsyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType);

private slots:
    void at_aboutToBeStopped();
    void at_aboutToBeStarted();
    void at_asyncRequestQueued(int operation, QnSessionManagerAsyncReplyProcessor* replyProcessor, const QUrl &url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray &data);
    void at_replyProcessor_finished(const QnHTTPRawResponse& response, int handle);

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void asyncRequestQueued(int operation, QnSessionManagerAsyncReplyProcessor* replyProcessor, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data);

private:
    QNetworkAccessManager *m_accessManager;
    mutable QMutex m_accessManagerMutex;
    static QAtomicInt s_handle;
    
    QScopedPointer<QThread> m_thread;
};

#endif // __SESSION_MANAGER_H__
