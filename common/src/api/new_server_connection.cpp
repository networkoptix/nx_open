#include "new_server_connection.h"

#include <api/app_server_connection.h>
#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <http/custom_headers.h>

#include <nx_ec/data/api_data.h>

#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/router.h>


// --------------------------- public methods -------------------------------------------

QnNewMediaServerConnection::QnNewMediaServerConnection(const QnUuid& serverId):
    m_serverId(serverId)
{

}

bool QnNewMediaServerConnection::cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback)
{
    return execute(lit("/ec2/cameraHistory"), request.toParams(), callback);
}

// --------------------------- private implementation -------------------------------------

QUrl QnNewMediaServerConnection::prepareUrl(const QString& path, const QnRequestParamList& params) const
{
    QUrl result;
    result.setPath(path);
    QUrlQuery q;
    for (const auto& param: params)
        q.addQueryItem(param.first, param.second);
    result.setQuery(q.toString());
    return result;
}

template <typename ResultType>
bool QnNewMediaServerConnection::execute(const QString& path, const QnRequestParamList& params, std::function<void (bool, ResultType)> callback) const
{
    return execute(prepareUrl(path, params), callback);
}

template <class T>
T parseMessageBody(const Qn::SerializationFormat& format, const nx_http::BufferType& msgBody, bool* success)
{
    switch(format) {
    case Qn::JsonFormat:
        return QJson::deserialized(msgBody, T(), success);
    case Qn::UbjsonFormat:
        return QnUbjson::deserialized(msgBody, T(), success);
    default:
        break;
    }
    return T();
}

template <typename ResultType>
bool QnNewMediaServerConnection::execute(const QUrl& url, std::function<void (bool, ResultType)> callback) const
{
    return sendRequest(m_serverId, url, [callback] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType contentType, nx_http::BufferType msgBody) 
    {
        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) 
        {
            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(contentType);
            bool success = false;
            ResultType result = parseMessageBody<ResultType>(format, msgBody, &success);
            callback(success, result);
        }
        else
            callback(false, ResultType());
    });
}

bool QnNewMediaServerConnection::sendRequest(const QnUuid& serverId, const QUrl& url, HttpCompletionFunc callback) const
{
    QnMediaServerResourcePtr server =  qnResPool->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return false;

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(url.path());
    apiUrl.setQuery(url.query());

    nx_http::HttpHeaders headers;
    auto videoWallGuid = QnAppServerConnectionFactory::videowallGuid();
    if (!videoWallGuid.isNull())
        headers.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videoWallGuid.toByteArray());
    headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray());
    headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, QnAppServerConnectionFactory::url().userName().toUtf8());
    headers.emplace("User-Agent", nx_http::userAgentString());

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (route.reverseConnect)
    {
        // no route exists. proxy via nearest server
        auto nearestServer = qnCommon->currentServer();
        if (!nearestServer)
            return false;
        QUrl nearestUrl(server->getApiUrl());
        if (nearestServer->getId() == qnCommon->moduleGUID())
            apiUrl.setHost(lit("127.0.0.1")); 
        else
            apiUrl.setHost(nearestUrl.host());
        apiUrl.setPort(nearestUrl.port());
    }
    else if (!route.addr.isNull())
    {
        apiUrl.setHost(route.addr.address.toString());
        apiUrl.setPort(route.addr.port);
    }

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    auto authType = nx_http::AsyncHttpClient::authBasicAndDigest;
    if (user.isEmpty() || password.isEmpty()) {
        if (QnUserResourcePtr admin = qnResPool->getAdministrator()) 
        {
            // if auth is not known, use admin hash
            user = admin->getName();
            password = QString::fromUtf8(admin->getDigest());
            authType = nx_http::AsyncHttpClient::authDigestWithPasswordHash;
        }
    }
    apiUrl.setUserName(user);
    apiUrl.setPassword(password);
    return nx_http::downloadFileAsyncEx( apiUrl, callback, headers, authType );
}
