#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>
#include <utils/common/request_param.h>

#include <rest/server/json_rest_result.h>

class TCPSocket;
class QnRestConnectionProcessor;

// TODO: This should go into nx::network::rest and placed in common/src/nx/network/rest.

enum class RestPermissions
{
    anyUser,
    adminOnly,
};

struct RestContent
{
    QByteArray type;
    QByteArray body;

    std::optional<QJsonValue> parse() const;
};

struct RestRequest
{
    // TODO: These should be private, all data should be avalible by getters.
    const nx::network::http::Request* httpRequest = nullptr;
    const QnRestConnectionProcessor* owner = nullptr;

    std::optional<RestContent> content;

    RestRequest(
        const nx::network::http::Request* httpRequest,
        const QnRestConnectionProcessor* owner);

    /**
     * Method from request, may be overwritten by:
     * - X-Method-Override header.
     * - method_ URL param if ini config allows it.
     */
    const nx::network::http::Method::ValueType& method() const;
    QString path() const;

    /**
     * If method may not provide message body, uses URL params.
     * If method provides message body, parses message body instead (url params may be merged into
     * it if ini config permits so).
     */
    const RequestParams& params() const;
    std::optional<QString> param(const QString& key) const;
    QString paramOr(const QString& key, const QString& defaultValue = {}) const;

    /**
     * If method may not provide message body, parses URL params.
     * If method provides message body, parses message body instead.
     */
    template <typename T>
    std::optional<T> parseContent() const;

    bool isExtraFormattingRequired() const;

private:
    nx::network::http::Method::ValueType calculateMethod() const;
    std::optional<QJsonValue> parseMessageBody() const;
    RequestParams calculateParams() const;
    std::optional<QJsonValue> calculateContent() const;

private:
    const RequestParams m_urlParams;
    const nx::network::http::Method::ValueType m_method;
    mutable std::optional<RequestParams> m_paramsCache;
};

struct RestResponse
{
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined;
    std::optional<RestContent> content;
    bool isUndefinedContentLength = false;
    nx::network::http::HttpHeaders httpHeaders;

    RestResponse(
        nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::undefined);

    static RestResponse result(const QnJsonRestResult& result);

    template<typename T>
    static RestResponse reply(const T& data);

    template<typename... Args>
    static RestResponse error(QnRestResult::Error errorCode, Args... args);
};

/**
 * QnRestRequestHandler must be thread-safe.
 *
 * Single handler instance receives all requests, each request in different thread.
 */
class QnRestRequestHandler: public QObject
{
public:
    virtual RestResponse executeRequest(const RestRequest& request);
    virtual void afterExecute(const RestRequest& request, const RestResponse& response);

    friend class QnRestProcessorPool;

    GlobalPermission permissions() const { return m_permissions; }

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const { return {}; }

protected:
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executeDelete(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request);
    virtual RestResponse executePut(const RestRequest& request);

    // TODO: Remove and reimplement by generic executeRequest.
    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/)
    {
        return nx::network::http::StatusCode::notImplemented;
    }
    virtual int executeDelete(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/)
    {
        return nx::network::http::StatusCode::notImplemented;
    }
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
    virtual int executePut(
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
    virtual void afterExecute(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QnRestConnectionProcessor* /*owner*/)
    {
    }

protected:
    void setPath(const QString& path) { m_path = path; }
    void setPermissions(GlobalPermission permissions ) { m_permissions = permissions; }
    QString extractAction(const QString& path) const;

protected:
    QString m_path;
    GlobalPermission m_permissions;
};

typedef QSharedPointer<QnRestRequestHandler> QnRestRequestHandlerPtr;

//--------------------------------------------------------------------------------------------------

template<typename T>
std::optional<T> RestRequest::parseContent() const
{
    const auto value = calculateContent();
    if (!value)
        return std::nullopt;

    T result;
    if (!QJson::deserialize(*value, &result))
        return std::nullopt;

    return result;
}

template<typename T>
RestResponse RestResponse::reply(const T& data)
{
    // TODO: Add support for other then JSON results?
    QnJsonRestResult json;
    json.setReply(data);
    return result(json);
}

inline void qStringListAdd(QStringList* /*list*/) {}

template<typename... Args>
void qStringListAdd(QStringList* list, QString arg, Args... args)
{
    list->append(std::move(arg));
    qStringListAdd(list, std::forward<Args>(args)...);
}

template<typename... Args>
RestResponse RestResponse::error(QnRestResult::Error errorCode, Args... args)
{
    QStringList argList;
    qStringListAdd(&argList, args...);

    QnRestResult rest;
    rest.setError(QnRestResult::ErrorDescriptor{errorCode, std::move(argList)});
    return result(rest);
}
