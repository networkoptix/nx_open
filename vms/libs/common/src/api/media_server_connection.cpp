#include "media_server_connection.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>

#include <nx/utils/uuid.h>

#include <api/helpers/chunks_request_data.h>

#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>

#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/ptz/ptz_data.h>

#include <nx_ec/data/api_conversion_functions.h>

#include <utils/common/ldap.h>
#include <utils/common/warnings.h>
#include <utils/common/request_param.h>

#include <nx/vms/api/data/email_settings_data.h>

#include <nx/fusion/model_functions.h>

#include <nx/network/http/http_types.h>

#include <api/app_server_connection.h>

#include <recording/time_period_list.h>

#include "network_proxy_factory.h"
#include "session_manager.h"
#include "media_server_reply_processor.h"

#include "model/camera_list_reply.h"
#include "model/configure_reply.h"
#include <nx/network/http/custom_headers.h>
#include "model/recording_stats_reply.h"
#include <api/model/getnonce_reply.h>
#include "common/common_module.h"

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>

using namespace nx;

namespace {

// TODO: Introduce constants for API methods registered in media_server_process.cpp.
QN_DEFINE_LEXICAL_ENUM(RequestObject,
    (PingSystemObject, "pingSystem")
    (GetNonceObject, "getRemoteNonce")
    (TestLdapSettingsObject, "testLdapSettings")
);

void trace(const QString& serverId, int handle, int obj, const QString& message = QString())
{
    static const nx::utils::log::Tag kTag(typeid(QnMediaServerConnection));
    NX_VERBOSE(kTag) << lm("%1 <%2>: %3 %4").args(
        serverId, handle, message, QnLexical::serialized(static_cast<RequestObject>(obj)));
}

nx::network::http::HttpHeaders makeDefaultHeaders()
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");

    return headers;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnMediaServerReplyProcessor

QnMediaServerReplyProcessor::QnMediaServerReplyProcessor(int object, const QString& serverId):
    QnAbstractReplyProcessor(object),
    m_serverId(serverId)
{
    timer.start();
}

void QnMediaServerReplyProcessor::processReply(const QnHTTPRawResponse& response, int handle)
{
    trace(m_serverId, handle, object(), lit("Received reply (%1ms)").arg(timer.elapsed()));
    switch (object())
    {
        case PingSystemObject:
            processJsonReply<nx::vms::api::ModuleInformation>(this, response, handle);
            break;
        case GetNonceObject:
            processJsonReply<QnGetNonceReply>(this, response, handle);
            break;
        case TestLdapSettingsObject:
            processJsonReply<QnLdapUsers>(this, response, handle);
            break;
        default:
            NX_ASSERT(false);
            break;
    }

    deleteLater();
}

//-------------------------------------------------------------------------------------------------
// QnMediaServerConnection

QnMediaServerConnection::QnMediaServerConnection(
    QnCommonModule* commonModule,
    const QnMediaServerResourcePtr& server,
    const QnUuid& videowallGuid,
    bool enableOfflineRequests)
:
    base_type(commonModule, server),
    m_server(server),
    m_serverVersion(server->getVersion()),
    m_proxyPort(0),
    m_enableOfflineRequests(enableOfflineRequests)
{
    setSerializer(QnLexical::newEnumSerializer<RequestObject, int>());
    setExtraQueryParameters(QnRequestParamList{{Qn::SERVER_GUID_HEADER_NAME, server->getId().toString()}});

    nx::network::http::HttpHeaders extraHeaders;
    extraHeaders.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getOriginalGuid().toByteArray());

    if (!videowallGuid.isNull())
        extraHeaders.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videowallGuid.toByteArray());
    extraHeaders.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        commonModule->runningInstanceGUID().toByteArray());
    if (const auto& connection = commonModule->ec2Connection())
    {
        extraHeaders.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME,
            connection->connectionInfo().ecUrl.userName().toUtf8());
    }
    extraHeaders.emplace(nx::network::http::header::kUserAgent, nx::network::http::userAgentString());
    setExtraHeaders(std::move(extraHeaders));
}

nx::utils::Url QnMediaServerConnection::url() const
{
    if (const auto server = m_server.lock())
        return server->getApiUrl();

    return nx::utils::Url();
}

QnAbstractReplyProcessor* QnMediaServerConnection::newReplyProcessor(int object, const QString& serverId)
{
    return new QnMediaServerReplyProcessor(object, serverId);
}

bool QnMediaServerConnection::isReady() const
{
    if (!targetResource())
        return false;

    if (m_enableOfflineRequests)
        return true;

    Qn::ResourceStatus status = targetResource()->getStatus();
    return status != Qn::Offline && status != Qn::NotDefined;
}

int QnMediaServerConnection::sendAsyncGetRequestLogged(
    int object,
    const QnRequestParamList& params,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    int handle = sendAsyncGetRequest(object, params, replyTypeName, target, slot, timeout);

    trace(handle, object, lm("GET %1 with timeout %2").args(
        replyTypeName, timeout ? *timeout : std::chrono::milliseconds{0}));

    return handle;
}

int QnMediaServerConnection::sendAsyncPostRequestLogged(
    int object,
    const QnRequestParamList& params,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncPostRequestLogged(
        object, makeDefaultHeaders(), {}, QJson::serialized(params.toJson()),
        replyTypeName, target, slot, timeout);
}

int QnMediaServerConnection::sendAsyncPostRequestLogged(
    int object,
    nx::network::http::HttpHeaders headers,
    const QnRequestParamList& params,
    const QByteArray& data,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    int handle = sendAsyncPostRequest(
        object, std::move(headers), params, data, replyTypeName, target, slot, timeout);

    trace(handle, object, lm("POST %1 with timeout %2").args(
        replyTypeName, timeout ? *timeout : std::chrono::milliseconds{0}));

    return handle;
}

void QnMediaServerConnection::trace(int handle, int obj, const QString& message /*= QString()*/)
{
    ::trace(m_serverId, handle, obj, message);
}

int QnMediaServerConnection::testLdapSettingsAsync(
    const QnLdapSettings& settings, QObject* target, const char* slot)
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");
    std::chrono::seconds timeout(settings.searchTimeoutS);
    return sendAsyncPostRequestLogged(TestLdapSettingsObject, std::move(headers),
        QnRequestParamList(), QJson::serialized(settings),
        QN_STRINGIZE_TYPE(QnLdapUsers), target, slot, timeout);
}

int QnMediaServerConnection::pingSystemAsync(
    const nx::utils::Url& url, const QString& getKey, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("url", url.toString());
    params.insert("getKey", getKey);

    return sendAsyncGetRequestLogged(PingSystemObject,
        params, QN_STRINGIZE_TYPE(nx::vms::api::ModuleInformation), target, slot);
}

int QnMediaServerConnection::getNonceAsync(const nx::utils::Url& url, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("url", url.toString());

    return sendAsyncGetRequest(GetNonceObject,
        params, QN_STRINGIZE_TYPE(QnGetNonceReply), target, slot);
}

