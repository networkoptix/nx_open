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


QnManualCameraAdditionRestHandler::QnManualCameraAdditionRestHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
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

int QnManualCameraAdditionRestHandler::searchStartAction(
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    NX_DEBUG(this, lm("Start searching new cameras"));

    QAuthenticator auth;
    auth.setUser(params.value("user", "admin"));
    auth.setPassword(params.value("password", "admin"));

    auto startIpValue = nx::network::HostAddress::ipV4from(params.value("start_ip"));
    auto endIpValue = nx::network::HostAddress::ipV4from(params.value("end_ip"));
    if (!startIpValue)
    {
        NX_WARNING(this, lm("Invalid value for parameter 'start_ip'"));
        return nx::network::http::StatusCode::unprocessableEntity;
    }

    nx::network::HostAddress startIp(startIpValue.get());
    std::optional<nx::network::HostAddress> endIp;
    if (endIpValue && endIpValue != startIpValue)
    {
        if (endIpValue->s_addr < startIpValue->s_addr)
        {
            NX_WARNING(this, lm("Invalid ip range, 'end_ip' must be greater than 'start_ip'"));
            return nx::network::http::StatusCode::unprocessableEntity;
        }
        endIp = nx::network::HostAddress(*endIpValue);
    }

    int port = 0;
    const auto portStr = params.value("port");
    if (!portStr.isEmpty())
    {
        bool ok;
        port = portStr.toUShort(&ok);
        if (!ok)
        {
            NX_WARNING(this, lm("Invalid value for parameter 'port'"));
            return nx::network::http::StatusCode::unprocessableEntity;
        }
    }

    QnUuid processUuid = QnUuid::createUuid();
    {
        NX_MUTEX_LOCKER lock(&m_searchProcessMutex);
        auto [searcher, inserted] = m_searchProcesses.try_emplace(processUuid,
                std::make_unique<QnManualCameraSearcher>(owner->commonModule()));
        if (!inserted)
            return nx::network::http::StatusCode::internalServerError;
        NX_VERBOSE(this, "Created search process with UUID=%1", processUuid);

        searcher->second->run(
            [this](QnManualCameraSearcher* searcher)
            {
                NX_VERBOSE(this, "Search (%1) was finished, found %2 resources",
                     searcher, searcher->status().cameras.size());
            },
            startIp, endIp, auth, port);
    }
    // NOTE: Finished searchers are removed only in in stop request and destructor!

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
        nx::utils::Url url(camera.url);
        if (url.host().isEmpty() && !url.path().isEmpty())
        {
            // camera.url is just an IP address or a hostname; QUrl parsed it as path, fixing it.
            url.setHost(url.path());
            url.setPath(QByteArray());
        }

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
            camera->saveParams();
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
