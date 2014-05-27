
#include "manual_camera_addition_rest_handler.h"

#include <boost/bind.hpp>
#include <thread>

#include <QtCore/QThreadPool>
#include <QtNetwork/QAuthenticator>

#include <api/model/manual_camera_seach_reply.h>
#include <core/resource_management/resource_discovery_manager.h>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/serialization/json_functions.h>


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
    }
};

static ManualSearchThreadPoolHolder manualSearchThreadPoolHolder;

QnManualCameraAdditionRestHandler::QnManualCameraAdditionRestHandler()
{
}

QnManualCameraAdditionRestHandler::~QnManualCameraAdditionRestHandler() {
    foreach (QnManualCameraSearcher* process, m_searchProcesses) {
        process->cancel();
        delete process;
    }
}

int QnManualCameraAdditionRestHandler::searchStartAction(const QnRequestParams &params,  QnJsonRestResult &result)
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

    QUuid processUuid = QUuid::createUuid();

    QnManualCameraSearcher* searcher = new QnManualCameraSearcher();
    {
        QMutexLocker lock(&m_searchProcessMutex);
        m_searchProcesses.insert(processUuid, searcher);
    }

    //TODO #ak better not to use concurrent here, since calling QtConcurrent::run from running task looks unreliable in some extreme case
        //consider using async fsm here (this one should be quite simple)
    //NOTE boost::bind is here temporarily: till QnConcurrent::run supports any number of arguments
    m_searchProcessRuns.insert( processUuid,
        QnConcurrent::run( 
            &manualSearchThreadPoolHolder.pool,
            boost::bind( &QnManualCameraSearcher::run, searcher, &manualSearchThreadPoolHolder.pool, addr1, addr2, auth, port) ) );

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}

int QnManualCameraAdditionRestHandler::searchStatusAction(const QnRequestParams &params, QnJsonRestResult &result) {
    QUuid processUuid = QUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    if (!isSearchActive(processUuid))
        return CODE_NOT_FOUND;

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}


int QnManualCameraAdditionRestHandler::searchStopAction(const QnRequestParams &params, QnJsonRestResult &result) {
    QUuid processUuid = QUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualCameraSearcher* process(NULL);
    if (isSearchActive(processUuid)) {
        QMutexLocker lock(&m_searchProcessMutex);
        process = m_searchProcesses[processUuid];
        process->cancel();
        m_searchProcesses.remove(processUuid);
    }

    m_searchProcessRuns[processUuid].waitForFinished();
    m_searchProcessRuns.remove(processUuid);

    QnManualCameraSearchReply reply;
    reply.processUuid = processUuid;
    if (process) {
        QnManualCameraSearchProcessStatus processStatus = process->status();
        reply.status = processStatus.status;
        reply.cameras = processStatus.cameras;

        delete process;
    }
    result.setReply(reply);

    return CODE_OK;
}


int QnManualCameraAdditionRestHandler::addCamerasAction(const QnRequestParams &params, QnJsonRestResult &result)
{
    QAuthenticator auth;
    auth.setUser(params.value("user"));
    auth.setPassword(params.value("password"));

    QnManualCameraInfoMap infos;
    for(int i = 0, skipped = 0; skipped < 5; i++) {
        QString url = params.value("url" + QString::number(i));
        QString manufacturer = params.value("manufacturer" + QString::number(i));

        if(url.isEmpty() || manufacturer.isEmpty()) {
            skipped++;
            continue;
        }

        QnManualCameraInfo info(url, auth, manufacturer);
        if(info.resType.isNull()) {
            result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid camera manufacturer '%1'.").arg(manufacturer));
            return CODE_INVALID_PARAMETER;
        }

        infos.insert(url, info);
    }

    bool registered = QnResourceDiscoveryManager::instance()->registerManualCameras(infos);
    result.setReply(registered);
    return registered ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    QString action = extractAction(path);
    if (action == "search")
        return searchStartAction(params, result);
    else if (action == "status")
        return searchStatusAction(params, result);
    else if (action == "stop")
        return searchStopAction(params, result);
    else if (action == "add")
        return addCamerasAction(params, result);
    else
        return CODE_NOT_FOUND;
}

QnManualCameraSearchProcessStatus QnManualCameraAdditionRestHandler::getSearchStatus(const QUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);

    if (!m_searchProcesses.contains(searchProcessUuid))
        return QnManualCameraSearchProcessStatus();

    return m_searchProcesses[searchProcessUuid]->status();
}

bool QnManualCameraAdditionRestHandler::isSearchActive(const QUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);
    return m_searchProcesses.contains(searchProcessUuid);
}

QString QnManualCameraAdditionRestHandler::description() const
{
    return 
            "Search or manual add cameras found in the specified range.<BR>\n"
            "<BR><b>api/manualCamera/search</b> - start camera searching"
            "<BR>Param <b>start_ip</b> - first ip address in range."
            "<BR>Param <b>end_ip</b> - end ip address in range. Can be omitted - only start ip address will be used"
            "<BR>Param <b>port</b> - Port to scan. Can be omitted"
            "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>"
            "<BR>Param <b>password</b> - password for the cameras. Can be omitted."
            "<BR><b>Return</b> XML with camera names, manufacturer and urls"
            "<BR>"
            "<BR><b>api/manualCamera/status</b> - get manual addition progress."
            "<BR>Param <b>uuid</b> - process uuid."
            "<BR>"
            "<BR><b>api/manualCamera/stop</b> - stop manual addition progress."
            "<BR>Param <b>uuid</b> - process uuid."
            "<BR>"
            "<BR><b>api/manualCamera/add</b> - manual add camera(s). If several cameras are added, parameters 'url' and 'manufacturer' must be defined several times"
            "<BR>Param <b>url</b> - camera url returned by scan request."
            "<BR>Param <b>manufacturer</b> - camera manufacturer.</i>"
            "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>"
            "<BR>Param <b>password</b> - password for the cameras. Can be omitted."
            ;
}
