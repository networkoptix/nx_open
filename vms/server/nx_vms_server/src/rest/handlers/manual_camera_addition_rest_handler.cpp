#include "manual_camera_addition_rest_handler.h"

#include <functional>

#include <QtCore/QThreadPool>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include "audit/audit_manager.h"

#include <core/resource_management/resource_discovery_manager.h>
#include "core/resource/network_resource.h"
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/fusion/serialization/json_functions.h>
#include <common/common_module.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/url/url_builder.h>


static constexpr const char kUserParam[] = "user";
static constexpr const char kPasswordParam[] = "password";
static constexpr const char kStartIpParam[] = "start_ip";
static constexpr const char kEndIpParam[] = "end_ip";
static constexpr const char kPortParam[] = "port";
static constexpr const char kUrlParam[] = "url";

QnManualCameraAdditionRestHandler::QnManualCameraAdditionRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnManualCameraAdditionRestHandler::~QnManualCameraAdditionRestHandler()
{
    NX_MUTEX_LOCKER lock(&m_searchProcessMutex);
    {
        nx::utils::BarrierWaiter barrier;
        for (auto& [uuid, searcher]: m_searchProcesses)
        {
            searcher->pleaseStop(barrier.fork());
        }
    }
}

int QnManualCameraAdditionRestHandler::extractSearchStartParams(QnJsonRestResult* const result,
    const QnRequestParams& params,
    nx::utils::Url* const outUrl,
    std::optional<std::pair<nx::network::HostAddress, nx::network::HostAddress>>* const outIpRange)
{
    NX_ASSERT(outUrl->isEmpty());
    NX_ASSERT(!outIpRange->has_value());

    const auto portStr = params.value(kPortParam);
    const auto urlStr = params.value(kUrlParam);
    const auto startIpStr = params.value(kStartIpParam);
    const auto endIpStr = params.value(kEndIpParam);

    if (!urlStr.isEmpty()) //< Single host search.
    {
        *outUrl = nx::utils::url::parseUrlFields(urlStr);
        if (!outUrl->isValid() || outUrl->host().isEmpty())
        {
            result->setError(QnRestResult::InvalidParameter,
                lm("Invalid value '%1' for parameter '%2'").args(urlStr, kUrlParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }

        if (!startIpStr.isEmpty() || !endIpStr.isEmpty())
        {
            result->setError(QnRestResult::InvalidParameter,
                lm("Parameter '%1' conflicts with '%2' and '%3' parameters")
                    .args(kUrlParam, kStartIpParam, kEndIpParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }

        if (!outUrl->password().isEmpty() || !outUrl->userName().isEmpty())
        {
            NX_DEBUG(this,
                "Credentials passed in '%1' are always overwritten by '%2' and '%3' params",
                kUrlParam, kPasswordParam, kUserParam);
        }
    }
    else if (!startIpStr.isEmpty() && !endIpStr.isEmpty()) //< IP range search.
    {
        const auto startIpValue = nx::network::HostAddress::ipV4from(startIpStr);
        const auto endIpValue = nx::network::HostAddress::ipV4from(endIpStr);
        if (!startIpValue || !endIpValue)
        {
            result->setError(QnRestResult::InvalidParameter,
                lm("Invalid ip range, '%1' and '%2' must be IP addresses")
                    .args(kEndIpParam, kStartIpParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }

        if (startIpValue->s_addr >= endIpValue->s_addr)
        {
            result->setError(QnRestResult::InvalidParameter,
                lm("Invalid ip range, '%1' must be greater than '%2'")
                    .args(kEndIpParam, kStartIpParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }

        *outIpRange = {nx::network::HostAddress(startIpValue.get()),
            nx::network::HostAddress(endIpValue.get())};

    }
    // Single host search deprecated API call (startIp is URL here and endIp empty).
    else if (!startIpStr.isEmpty() && endIpStr.isEmpty())
    {
        // NOTE: This branch of the condition should be removed when the support of the deprecated
        // API is ended.
        NX_WARNING(this, "Got the request using the deprecated API");
        *outUrl = nx::utils::url::parseUrlFields(startIpStr);
        if (!outUrl->isValid() || outUrl->host().isEmpty())
        {
            result->setError(QnRestResult::InvalidParameter,
                lm("Invalid value '%1' for parameter '%2'").args(startIpStr, kStartIpParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }
    }
    else
    {
        result->setError(QnRestResult::InvalidParameter);
        return nx::network::http::StatusCode::unprocessableEntity;
    }

    if (!portStr.isEmpty())
    {
        bool ok;
        const int port = portStr.toUShort(&ok);
        if (!ok)
        {
            result->setError(QnRestResult::InvalidParameter, lm("Invalid value for parameter '%1'")
                .args(kPortParam));
            return nx::network::http::StatusCode::unprocessableEntity;
        }
        outUrl->setPort(port);
    }

    const auto userStr = params.value(kUserParam);
    const auto passwordStr = params.value(kPasswordParam);
    if (!userStr.isEmpty() || !passwordStr.isEmpty())
    {
        outUrl->setUserName(userStr);
        outUrl->setPassword(passwordStr);
    }
    return nx::network::http::StatusCode::ok;
}

int QnManualCameraAdditionRestHandler::searchStartAction(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_DEBUG(this, lm("Start searching for new cameras"));

    nx::utils::Url url;
    std::optional<std::pair<nx::network::HostAddress, nx::network::HostAddress>> ipRange;
    const auto returnCode = extractSearchStartParams(&result, params, &url, &ipRange);
    if (returnCode != nx::network::http::StatusCode::ok)
        return returnCode;

    QnUuid processUuid = QnUuid::createUuid();
    {
        NX_MUTEX_LOCKER lock(&m_searchProcessMutex);
        auto [searcher, inserted] = m_searchProcesses.try_emplace(processUuid,
                std::make_unique<QnManualCameraSearcher>(owner->commonModule()));
        if (!inserted)
            return nx::network::http::StatusCode::internalServerError;
        NX_VERBOSE(this, "Created search process with UUID=%1", processUuid);

        auto handler =
            [this](QnManualCameraSearcher* searcher)
            {
                NX_VERBOSE(this, "Search (%1) was finished, found %2 resources",
                     searcher, searcher->status().cameras.size());
            };

        if (ipRange.has_value())
            searcher->second->startRangeSearch(handler, ipRange->first, ipRange->second, url);
        else
            searcher->second->startSearch(handler, url);
    }
    // NOTE: Finished searchers are removed only in stop request and destructor!

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);
    NX_DEBUG(this, "New cameras search was initiated");
    return nx::network::http::StatusCode::ok;
}

int QnManualCameraAdditionRestHandler::searchStatusAction(
    const QnRequestParams& params, QnJsonRestResult& result)
{
    QnUuid processUuid = QnUuid(params.value("uuid"));
    NX_VERBOSE(this, "Status of the search %1 was requested", processUuid);

    if (processUuid.isNull())
        return nx::network::http::StatusCode::unprocessableEntity;

    if (!isSearchActive(processUuid))
        return nx::network::http::StatusCode::notFound;

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnManualCameraAdditionRestHandler::searchStopAction(
    const QnRequestParams& params, QnJsonRestResult& result)
{
    QnUuid processUuid = QnUuid(params.value("uuid"));
    if (processUuid.isNull())
        return nx::network::http::StatusCode::unprocessableEntity;

    NX_VERBOSE(this, "Stopping search process with UUID=%1", processUuid);
    QnManualCameraSearcherPtr searcher;
    {
        NX_MUTEX_LOCKER lock(&m_searchProcessMutex);
        if (m_searchProcesses.count(processUuid))
        {
            searcher = std::move(m_searchProcesses[processUuid]);
            searcher->pleaseStopSync(); // TODO: #dliman Use async?
            m_searchProcesses.erase(processUuid);
        }
    }

    QnManualCameraSearchReply reply;
    reply.processUuid = processUuid;
    if (searcher)
    {
        QnManualCameraSearchProcessStatus processStatus = searcher->status();
        reply.status = processStatus.status;
        reply.cameras = processStatus.cameras;
    }
    result.setReply(reply);

    return nx::network::http::StatusCode::ok;
}

int QnManualCameraAdditionRestHandler::addCamerasAction(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    AddManualCameraData data;

    data.user = params.value("user");
    data.password = params.value("password");

    for (int i = 0, skipped = 0; skipped < 5; ++i)
    {
        ManualCameraData camera;
        camera.url = params.value("url" + QString::number(i));
        camera.manufacturer = params.value("manufacturer" + QString::number(i));

        if(camera.url.isEmpty() || camera.manufacturer.isEmpty()) {
            skipped++;
            continue;
        }
        camera.uniqueId = params.value("uniqueId" + QString::number(i));

        data.cameras.append(camera);
    }

    return addCameras(data, result, owner);
}

int QnManualCameraAdditionRestHandler::addCameras(
    const AddManualCameraData& data,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QAuthenticator auth;
    auth.setUser(data.user);
    auth.setPassword(data.password);

    std::vector<QnManualCameraInfo> cameraList;
    for (const auto& camera: data.cameras)
    {
        const auto url = nx::utils::url::parseUrlFields(camera.url);
        QnManualCameraInfo info(url, auth, camera.manufacturer, camera.uniqueId);
        if (info.resType.isNull())
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Invalid camera manufacturer '%1'.").arg(camera.manufacturer));
            return nx::network::http::StatusCode::unprocessableEntity;
        }

        cameraList.push_back(info);
    }

    auto registered = owner->commonModule()->resourceDiscoveryManager()->registerManualCameras(cameraList);
    auto resPool = owner->commonModule()->resourcePool();
    for (const auto& id: registered)
    {
        if (auto camera = resPool->getResourceByUniqueId<QnSecurityCamResource>(id))
        {
            camera->setAuth(auth);
            camera->saveProperties();
        }

    }
    if (!registered.isEmpty())
    {
        QnAuditRecord auditRecord =
            auditManager()->prepareRecord(owner->authSession(), Qn::AR_CameraInsert);
        for (const QnManualCameraInfo& info: cameraList)
        {
            if (!info.uniqueId.isEmpty())
                auditRecord.resources.push_back(QnNetworkResource::physicalIdToId(info.uniqueId));
        }
        owner->commonModule()->auditManager()->addAuditRecord(auditRecord);
    }

    if (registered.size() > 0)
        return nx::network::http::StatusCode::ok;
    else
        return nx::network::http::StatusCode::internalServerError;
}

int QnManualCameraAdditionRestHandler::executeGet(
    const QString &path, const QnRequestParams &params, QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "search")
        return searchStartAction(params, result, owner);
    else if (action == "status")
        return searchStatusAction(params, result);
    else if (action == "stop")
        return searchStopAction(params, result);
    else if (action == "add")
        return addCamerasAction(params, result, owner);
    else
        return nx::network::http::StatusCode::notFound;
}

int QnManualCameraAdditionRestHandler::executePost(
    const QString& path, const QnRequestParams& /*params*/, const QByteArray& body,
    QnJsonRestResult& result, const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "add")
    {
        const AddManualCameraData data = QJson::deserialized<AddManualCameraData>(body);
        return addCameras(data, result, owner);
    }
    else
    {
        return nx::network::http::StatusCode::notFound;
    }
}

QnManualCameraSearchProcessStatus QnManualCameraAdditionRestHandler::getSearchStatus(
    const QnUuid& searchProcessUuid)
{
    NX_MUTEX_LOCKER lock(&m_searchProcessMutex);

    if (!m_searchProcesses.count(searchProcessUuid))
        return QnManualCameraSearchProcessStatus();

    return m_searchProcesses[searchProcessUuid]->status();
}

bool QnManualCameraAdditionRestHandler::isSearchActive(
    const QnUuid &searchProcessUuid)
{
    NX_MUTEX_LOCKER lock(&m_searchProcessMutex);
    return m_searchProcesses.count(searchProcessUuid);
}
