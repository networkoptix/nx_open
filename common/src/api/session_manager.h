#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <future>

#include <boost/optional.hpp>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "utils/common/request_param.h"


struct AsyncRequestInfo
{
    AsyncRequestInfo()
    :
        handle(0),
        object(nullptr),
        slot(nullptr),
        connectionType(Qt::AutoConnection),
        requestedCompletedPromise(nullptr)
    {
    }

    int handle;
    QObject* object;
    const char* slot;
    Qt::ConnectionType connectionType;
    nx::utils::promise<QnHTTPRawResponse>* requestedCompletedPromise;
};
Q_DECLARE_METATYPE(AsyncRequestInfo);

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
    QNetworkReply::NetworkError sendSyncRequest(
        nx_http::Method::ValueType method,
        const QUrl& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        QByteArray msgBody,
        QnHTTPRawResponse& response);
    int sendSyncGetRequest(const QUrl& url, const QString &objectName, QnHTTPRawResponse& response);
    int sendSyncGetRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const QUrl& url, const QString &objectName, QByteArray msgBody, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QnHTTPRawResponse& response);
    QByteArray lastError() const;

    /** Asynchronous requests return request handle
        @param objectName Name of api request (actually, part of url)
        @param params Request parameters. Added to request's url query section
        @param target Allowed to be NULL, if result is not required for calling party
        @return Request handle
    */
    int sendAsyncRequest(
        nx_http::Method::ValueType method,
        const QUrl& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        QByteArray msgBody,
        QObject *target,
        const char *slot,
        Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const QUrl& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);

private:
    QUrl createApiUrl(
        const QUrl& baseUrl,
        const QString &objectName,
        const QnRequestParamList &params = QnRequestParamList()) const;
    int sendAsyncRequest(
        nx_http::Method::ValueType method,
        const QUrl& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        const QByteArray& msgBody,
        AsyncRequestInfo requestInfo);

private slots:
    void onHttpClientDone(nx_http::AsyncHttpClientPtr clientPtr);

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void asyncRequestQueued(nx_http::Method::ValueType operation, AsyncRequestInfo reqInfo, const QUrl& url, const QString &objectName, const nx_http::HttpHeaders &headers, const QnRequestParamList &params, const QByteArray& msgBody);

private:
    mutable QnMutex m_mutex;
    static QAtomicInt s_handle;

    QScopedPointer<QThread> m_thread;
    std::map<nx_http::AsyncHttpClientPtr, AsyncRequestInfo> m_requestInProgress;
};

#endif // __SESSION_MANAGER_H__
