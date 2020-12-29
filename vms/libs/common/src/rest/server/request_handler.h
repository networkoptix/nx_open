#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>
#include <utils/common/request_param.h>

class TCPSocket;
class QnRestConnectionProcessor;

// TODO: #MSAPI header-class naming inconsistency. As all derived classes are named XyzRestHandler,
// I suggest to rename this one to either QnAbstractRestHandler or simply to QnRestHandler. And
// rename the header.

enum class RestPermissions
{
    anyUser,
    adminOnly,
};

struct RestRequest
{
    QString path;
    QnRequestParamList params; //< Should be QnRequestParams
    const QnRestConnectionProcessor* owner = nullptr; //< Better to pass data instead of owner
    const nx::network::http::Request* httpRequest = nullptr;

    RestRequest(
        QString path = {},
        QnRequestParamList params = {},
        const QnRestConnectionProcessor* owner = nullptr,
        const nx::network::http::Request* httpRequest = nullptr);
};

struct RestContent
{
    QByteArray type;
    QByteArray body;

    RestContent(QByteArray type = {}, QByteArray body = {});
};

struct RestResponse
{
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined;
    RestContent content;
    bool isUndefinedContentLength = false;
    nx::network::http::HttpHeaders httpHeaders;

    RestResponse(nx::network::http::StatusCode::Value statusCode= nx::network::http::StatusCode::undefined,
        RestContent content = {}, bool isUndefinedContentLength = false);
};

using DeviceIdRetriever = std::function<QString(const nx::network::http::Request&)>;

/**
 * QnRestRequestHandler must be thread-safe.
 *
 * Single handler instance receives all requests, each request in different thread.
 */
class QnRestRequestHandler: public QObject
{
    Q_OBJECT

public:
    QnRestRequestHandler();

    virtual RestResponse executeRequest(
        nx::network::http::Method::ValueType method,
        const RestRequest& request,
        const RestContent& content);

    // TODO: By default these methods use protected virtual functions, which should be removed.
    // Only new functions are supposed to be used in future
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executeDelete(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request, const RestContent& content);
    virtual RestResponse executePut(const RestRequest& request, const RestContent& content);
    virtual void afterExecute(const RestRequest& request, const QByteArray& response);

    friend class QnRestProcessorPool;

    GlobalPermission permissions() const { return m_permissions; }

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const { return {}; }

    virtual DeviceIdRetriever createCustomDeviceIdRetriever() const
    {
        return {};
    }

protected:
    /**
     * @return Http status code.
     */
    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/)
    {
        return nx::network::http::StatusCode::notImplemented;
    }

    /**
     * @return HTTP status code.
     */
    virtual int executeDelete(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner);

    /**
     * @return HTTP status code.
     */
    virtual int executePost(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QByteArray& /*srcBodyContentType*/,
        QByteArray& /*result*/,
        QByteArray& /*resultContentType*/,
        const QnRestConnectionProcessor* /*owner*/)
    {
        return nx::network::http::StatusCode::notImplemented;
    }

    /**
     * @return HTTP status code.
     */
    virtual int executePut(
            const QString& path,
            const QnRequestParamList& params,
            const QByteArray& body,
            const QByteArray& srcBodyContentType,
            QByteArray& result,
            QByteArray& resultContentType,
            const QnRestConnectionProcessor* owner);

    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner);

protected:
    void setPath(const QString& path) { m_path = path; }
    void setPermissions(GlobalPermission permissions ) { m_permissions = permissions; }
    QString extractAction(const QString& path) const;

protected:
    QString m_path;
    GlobalPermission m_permissions;
};

typedef QSharedPointer<QnRestRequestHandler> QnRestRequestHandlerPtr;

class QnRestGUIRequestHandlerPrivate;

class QnRestGUIRequestHandler: public QnRestRequestHandler
{
    Q_OBJECT

public:
    QnRestGUIRequestHandler();
    virtual ~QnRestGUIRequestHandler();

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* /*owner*/) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* /*owner*/) override;

protected:
    virtual int executeGetGUI(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result) = 0;

    virtual int executePostGUI(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        QByteArray& result) = 0;

private:
    Q_INVOKABLE void methodExecutor();

protected:
    Q_DECLARE_PRIVATE(QnRestGUIRequestHandler);

    QnRestGUIRequestHandlerPrivate* d_ptr;
};
