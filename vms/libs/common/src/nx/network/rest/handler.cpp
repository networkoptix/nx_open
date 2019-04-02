#include <nx/network/rest/handler.h>

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>

namespace nx::network::rest {

Response Handler::executeRequest(
    const Request& request)
{
    Response response;
    try
    {
        if (request.method() == nx::network::http::Method::get)
            response = executeGet(request);
        else if (request.method() == nx::network::http::Method::post)
            response = executePost(request);
        else if (request.method() == nx::network::http::Method::put)
            response = executePut(request);
        else if (request.method() == nx::network::http::Method::delete_)
            response = executeDelete(request);
        else
            throw Exception(QnRestResult::CantProcessRequest, "Invalid HTTP method");
    }
    catch (const Exception& error)
    {
        return Response::error(error.descriptor);
    }

    if (response.content && response.content->type == kJsonContentType
        && request.isExtraFormattingRequired())
    {
        response.content->body = nx::utils::formatJsonString(response.content->body);
    }

    return response;
}

void Handler::afterExecute(const Request& /*request*/, const Response& /*response*/)
{
}

GlobalPermission Handler::permissions() const
{
    return m_permissions;
}

void Handler::setPath(const QString& path)
{
    m_path = path;
}

void Handler::setPermissions(GlobalPermission permissions)
{
    m_permissions = permissions;
}

QStringList Handler::cameraIdUrlParams() const
{
    return {};
}

QString Handler::extractAction(const QString& path) const
{
    QString localPath = path;
    while(localPath.endsWith(L'/'))
        localPath.chop(1);
    return localPath.mid(localPath.lastIndexOf(L'/') + 1);
}

Response Handler::executeGet(const Request& request)
{
    return {nx::network::http::StatusCode::notImplemented};
}

Response Handler::executeDelete(const Request& request)
{
    return {nx::network::http::StatusCode::notImplemented};
}

Response Handler::executePost(const Request& request)
{
    return {nx::network::http::StatusCode::notImplemented};
}

Response Handler::executePut(const Request& request)
{
    return {nx::network::http::StatusCode::notImplemented};
}

} // namespace nx::network::rest

