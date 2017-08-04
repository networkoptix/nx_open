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

    RestRequest(QString path = {}, QnRequestParamList params = {},
        const QnRestConnectionProcessor* owner = nullptr);
};

struct RestContent
{
    QByteArray type;
    QByteArray body;

    RestContent(QByteArray type = {}, QByteArray body = {});
};

struct RestResponse
{
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
    RestContent content;
    bool isUndefinedContentLength = false;

    RestResponse(nx_http::StatusCode::Value statusCode= nx_http::StatusCode::undefined,
        RestContent content = {}, bool isUndefinedContentLength = false);
};

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

    // TODO: By default these methods use protected virtual functions, which should be removed.
    // Only new functions are supposed to be used in future
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executeDelete(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request, const RestContent& content);
    virtual RestResponse executePut(const RestRequest& request, const RestContent& content);
    virtual void afterExecute(const RestRequest& request, const QByteArray& response);

    friend class QnRestProcessorPool;

    Qn::GlobalPermission permissions() const { return m_permissions; }

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const { return {}; }

    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner);

protected:
    /**
     * @return Http status code.
     */
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) = 0;

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
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* owner) = 0;

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

protected:
    void setPath(const QString& path) { m_path = path; }
    void setPermissions(Qn::GlobalPermission permissions ) { m_permissions = permissions; }
    QString extractAction(const QString& path) const;

protected:
    QString m_path;
    Qn::GlobalPermission m_permissions;
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
