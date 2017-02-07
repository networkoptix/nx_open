#include "manual_camera_addition_rest_handler.h"

#include <boost/bind.hpp>

#include <QtCore/QThreadPool>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include "audit/audit_manager.h"

#include <core/resource_management/resource_discovery_manager.h>
#include "core/resource/network_resource.h"
#include <network/tcp_connection_priv.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/fusion/serialization/json_functions.h>

class ManualSearchThreadPoolHolder
{
public:
    QThreadPool pool;

    ManualSearchThreadPoolHolder()
    {
        pool.setMaxThreadCount(
            #ifdef __arm__
                8
            #else
                32
            #endif
        );
        // Default expiration timeout is 30 seconds, but there is a bug in QT older than v.5.3.
        pool.setExpiryTimeout(-1);
    }
};

static ManualSearchThreadPoolHolder manualSearchThreadPoolHolder;

QnManualCameraAdditionRestHandler::QnManualCameraAdditionRestHandler()
{
}

QnManualCameraAdditionRestHandler::~QnManualCameraAdditionRestHandler()
{
    for (QnManualCameraSearcher* process: m_searchProcesses)
    {
        process->cancel();
        delete process;
    }
}

int QnManualCameraAdditionRestHandler::searchStartAction(
    const QnRequestParams& params, QnJsonRestResult& result)
{
    QAuthenticator auth;
    auth.setUser(params.value("user", "admin"));
    auth.setPassword(params.value("password", "admin"));

    QString addr1 = params.value("start_ip");
    QString addr2 = params.value("end_ip");

    int port = params.value("port").toInt();

    if (addr1.isNull())
        return CODE_INVALID_PARAMETER;

    if (addr2 == addr1)
        addr2.clear();

    QnUuid processUuid = QnUuid::createUuid();

    QnManualCameraSearcher* searcher = new QnManualCameraSearcher();

    {
        QnMutexLocker lock( &m_searchProcessMutex );
        m_searchProcesses.insert(processUuid, searcher);

        // TODO: #ak: better not to use concurrent here, since calling QtConcurrent::run from
        // running task looks unreliable in some extreme case.
        // Consider using async fsm here (this one should be quite simple).
        // NOTE: boost::bind is here temporarily, until QnConcurrent::run supports arbitrary
        // number of arguments.
        m_searchProcessRuns.insert(processUuid,
            QnConcurrent::run(
                &manualSearchThreadPoolHolder.pool,
                boost::bind(
                    &QnManualCameraSearcher::run,
                    searcher,
                    &manualSearchThreadPoolHolder.pool,
                    addr1,
                    addr2,
                    auth,
                    port)));
    }

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}

int QnManualCameraAdditionRestHandler::searchStatusAction(
    const QnRequestParams& params, QnJsonRestResult& result)
{
    QnUuid processUuid = QnUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    if (!isSearchActive(processUuid))
        return CODE_NOT_FOUND;

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}


int QnManualCameraAdditionRestHandler::searchStopAction(
    const QnRequestParams& params, QnJsonRestResult& result)
{
    QnUuid processUuid = QnUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualCameraSearcher* process(NULL);
    {
        QnMutexLocker lock( &m_searchProcessMutex );
        if (m_searchProcesses.contains(processUuid))
        {
            process = m_searchProcesses[processUuid];
            process->cancel();
            m_searchProcesses.remove(processUuid);
        }
        if (m_searchProcessRuns.contains(processUuid))
        {
            m_searchProcessRuns[processUuid].waitForFinished();
            m_searchProcessRuns.remove(processUuid);
        }
    }

    QnManualCameraSearchReply reply;
    reply.processUuid = processUuid;
    if (process)
    {
        QnManualCameraSearchProcessStatus processStatus = process->status();
        reply.status = processStatus.status;
        reply.cameras = processStatus.cameras;

        delete process;
    }
    result.setReply(reply);

    return CODE_OK;
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

    QnManualCameraInfoMap infoMap;
    for (const auto& camera: data.cameras)
    {
        QUrl url(camera.url);
        if (url.host().isEmpty() && !url.path().isEmpty())
        {
            // camera.url is just an IP address or a hostname; QUrl parsed it as path, fixing it.
            url.setHost(url.path());
            url.setPath(QByteArray());
        }

        QnManualCameraInfo info(url, auth, camera.manufacturer);
        info.uniqueId = camera.uniqueId;
        if (info.resType.isNull())
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Invalid camera manufacturer '%1'.").arg(camera.manufacturer));
            return CODE_INVALID_PARAMETER;
        }

        infoMap.insert(camera.url, info);
    }

    int registered = QnResourceDiscoveryManager::instance()->registerManualCameras(infoMap);
    if (registered > 0)
    {
        QnAuditRecord auditRecord =
            qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_CameraInsert);
        for (const QnManualCameraInfo& info: infoMap)
        {
            if (!info.uniqueId.isEmpty())
                auditRecord.resources.push_back(QnNetworkResource::physicalIdToId(info.uniqueId));
        }
        qnAuditManager->addAuditRecord(auditRecord);
    }

    return registered > 0 ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionRestHandler::executeGet(
    const QString &path, const QnRequestParams &params, QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    QString action = extractAction(path);
    if (action == "search")
        return searchStartAction(params, result);
    else if (action == "status")
        return searchStatusAction(params, result);
    else if (action == "stop")
        return searchStopAction(params, result);
    else if (action == "add")
        return addCamerasAction(params, result, owner);
    else
        return CODE_NOT_FOUND;
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
        return CODE_NOT_FOUND;
    }
}

QnManualCameraSearchProcessStatus QnManualCameraAdditionRestHandler::getSearchStatus(
    const QnUuid& searchProcessUuid)
{
    QnMutexLocker lock(&m_searchProcessMutex);

    if (!m_searchProcesses.contains(searchProcessUuid))
        return QnManualCameraSearchProcessStatus();

    return m_searchProcesses[searchProcessUuid]->status();
}

bool QnManualCameraAdditionRestHandler::isSearchActive(
    const QnUuid &searchProcessUuid)
{
    QnMutexLocker lock(&m_searchProcessMutex);
    return m_searchProcesses.contains(searchProcessUuid);
}
