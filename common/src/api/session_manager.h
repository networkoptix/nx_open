#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QTimer>

#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QSslError>

#include "utils/common/request_param.h"


class QNetworkReply;
class QNetworkAccessManager;

struct AsyncRequestInfo {

    AsyncRequestInfo(): handle(0), object(0), slot(0), connectionType(Qt::AutoConnection) {}

    int handle;
    QObject* object;
    const char* slot;
    Qt::ConnectionType connectionType;
};
Q_DECLARE_METATYPE(AsyncRequestInfo);

class QnSessionManagerSyncReply
{
public:
    QnSessionManagerSyncReply(): m_finished(false) {}

    int wait(QnHTTPRawResponse &response);
    void terminate();
    void requestFinished(const QnHTTPRawResponse& response, int handle);
private:
    bool m_finished;
    QnHTTPRawResponse m_response;
    QMutex m_mutex;
    QWaitCondition m_condition;
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
    int sendSyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response);
    int sendSyncGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response);
    int sendSyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QnHTTPRawResponse& response);
    QByteArray lastError() const;

    // Asynchronous requests return request handle
    /*!
        \param target Allowed to be NULL, if result is not required for calling party
    */
    int sendAsyncRequest(int operation, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, int id, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncDeleteRequest(const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);

private:
    QUrl createApiUrl(const QUrl& baseUrl, const QString &objectName, const QnRequestParamList &params = QnRequestParamList()) const;

private slots:
    void at_authenticationRequired(QNetworkReply* reply, QAuthenticator * authenticator);
    void at_proxyAuthenticationRequired ( const QNetworkProxy & proxy, QAuthenticator * authenticator);
    void at_aboutToBeStopped();
    void at_aboutToBeStarted();
    void at_asyncRequestQueued(int operation, AsyncRequestInfo reqInfo, const QUrl &url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray &data);
    void at_sslErrors(QNetworkReply* reply, const QList<QSslError> &errors);
    void at_replyReceived(QNetworkReply * reply);
    void at_SyncRequestFinished(const QnHTTPRawResponse& response, int handle);

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void asyncRequestQueued(int operation, AsyncRequestInfo reqInfo, const QUrl& url, const QString &objectName, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data);

private:
    QNetworkAccessManager *m_accessManager;
    mutable QMutex m_accessManagerMutex;
    static QAtomicInt s_handle;
    
    QScopedPointer<QThread> m_thread;
    QHash<QNetworkReply*, AsyncRequestInfo> m_handleInProgress;

    QHash<int, QnSessionManagerSyncReply*>  m_syncReplyInProgress;
    QMutex m_syncReplyMutex;
};

#endif // __SESSION_MANAGER_H__
