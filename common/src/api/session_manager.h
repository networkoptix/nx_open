#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include <future>

#include <boost/optional.hpp>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "utils/common/request_param.h"
#include <nx/utils/safe_direct_connection.h>
#include <common/common_module_aware.h>

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
class QnSessionManager:
    public QObject,
    public QnCommonModuleAware,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    QnSessionManager(QObject* parent);
    virtual ~QnSessionManager();

    static QByteArray formatNetworkError(int error);

    void stop();
    void start();
    bool isReady() const;

    // Synchronous requests return status
    QNetworkReply::NetworkError sendSyncRequest(
        nx_http::Method::ValueType method,
        const nx::utils::Url& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        QByteArray msgBody,
        QnHTTPRawResponse& response);
    int sendSyncGetRequest(const nx::utils::Url& url, const QString &objectName, QnHTTPRawResponse& response);
    int sendSyncGetRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const nx::utils::Url& url, const QString &objectName, QByteArray msgBody, QnHTTPRawResponse& response);
    int sendSyncPostRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QnHTTPRawResponse& response);
    QByteArray lastError() const;

    /** Asynchronous requests return request handle
        @param objectName Name of api request (actually, part of url)
        @param params Request parameters. Added to request's url query section
        @param target Allowed to be NULL, if result is not required for calling party
        @return Request handle
    */
    int sendAsyncRequest(
        nx_http::Method::ValueType method,
        const nx::utils::Url& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        QByteArray msgBody,
        QObject *target,
        const char *slot,
        Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const nx::utils::Url& url, const QString &objectName, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncGetRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const nx::utils::Url& url, const QString &objectName, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);
    int sendAsyncPostRequest(const nx::utils::Url& url, const QString &objectName, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QObject *target, const char *slot, Qt::ConnectionType connectionType = Qt::AutoConnection);

private:
    nx::utils::Url createApiUrl(
        const nx::utils::Url& baseUrl,
        const QString &objectName,
        const QnRequestParamList &params = QnRequestParamList()) const;
    int sendAsyncRequest(
        nx_http::Method::ValueType method,
        const nx::utils::Url& url,
        const QString &objectName,
        nx_http::HttpHeaders headers,
        const QnRequestParamList &params,
        const QByteArray& msgBody,
        AsyncRequestInfo requestInfo);

private slots:
    void onHttpClientDone(int requestId, nx_http::AsyncHttpClientPtr clientPtr);

signals:
    void aboutToBeStopped();
    void aboutToBeStarted();
    void asyncRequestQueued(nx_http::Method::ValueType operation, AsyncRequestInfo reqInfo, const QUrl& url, const QString &objectName, const nx_http::HttpHeaders &headers, const QnRequestParamList &params, const QByteArray& msgBody);

private:
    mutable QnMutex m_mutex;

    QScopedPointer<QThread> m_thread;
    std::map<int, AsyncRequestInfo> m_requestInProgress;
};

#endif // __SESSION_MANAGER_H__
