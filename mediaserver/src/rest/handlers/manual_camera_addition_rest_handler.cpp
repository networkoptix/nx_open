
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
    for (QnManualCameraSearcher* process: m_searchProcesses) {
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

    QnUuid processUuid = QnUuid::createUuid();

    QnManualCameraSearcher* searcher = new QnManualCameraSearcher();
    {
        QMutexLocker lock(&m_searchProcessMutex);
        m_searchProcesses.insert(processUuid, searcher);

        //TODO #ak better not to use concurrent here, since calling QtConcurrent::run from running task looks unreliable in some extreme case
            //consider using async fsm here (this one should be quite simple)
        //NOTE boost::bind is here temporarily: till QnConcurrent::run supports any number of arguments
        m_searchProcessRuns.insert( processUuid,
            QnConcurrent::run( 
                &manualSearchThreadPoolHolder.pool,
                boost::bind( &QnManualCameraSearcher::run, searcher, &manualSearchThreadPoolHolder.pool, addr1, addr2, auth, port) ) );
    }

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}

int QnManualCameraAdditionRestHandler::searchStatusAction(const QnRequestParams &params, QnJsonRestResult &result) {
    QnUuid processUuid = QnUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    if (!isSearchActive(processUuid))
        return CODE_NOT_FOUND;

    QnManualCameraSearchReply reply(processUuid, getSearchStatus(processUuid));
    result.setReply(reply);

    return CODE_OK;
}


int QnManualCameraAdditionRestHandler::searchStopAction(const QnRequestParams &params, QnJsonRestResult &result) {
    QnUuid processUuid = QnUuid(params.value("uuid"));

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualCameraSearcher* process(NULL);
    {
        QMutexLocker lock(&m_searchProcessMutex);
        if (m_searchProcesses.contains(processUuid)) 
        {
            process = m_searchProcesses[processUuid];
            process->cancel();
            m_searchProcesses.remove(processUuid);
        }
        if (m_searchProcessRuns.contains(processUuid)) {
            m_searchProcessRuns[processUuid].waitForFinished();
            m_searchProcessRuns.remove(processUuid);
        }
    }

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
        QString urlStr = params.value("url" + QString::number(i));
        QString manufacturer = params.value("manufacturer" + QString::number(i));

        if(urlStr.isEmpty() || manufacturer.isEmpty()) {
            skipped++;
            continue;
        }

        QUrl url( urlStr );
        if( url.host().isEmpty() && !url.path().isEmpty() )
        {
            //urlStr is just an ip address or a hostname, QUrl parsed it as path, restoring justice...
            url.setHost( url.path() );
            url.setPath( QByteArray() );
        }

        QnManualCameraInfo info(url, auth, manufacturer);
        if(info.resType.isNull()) {
            result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid camera manufacturer '%1'.").arg(manufacturer));
            return CODE_INVALID_PARAMETER;
        }

        infos.insert(urlStr, info);
    }

    bool registered = QnResourceDiscoveryManager::instance()->registerManualCameras(infos);
    result.setReply(registered);
    return registered ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
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

QnManualCameraSearchProcessStatus QnManualCameraAdditionRestHandler::getSearchStatus(const QnUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);

    if (!m_searchProcesses.contains(searchProcessUuid))
        return QnManualCameraSearchProcessStatus();

    return m_searchProcesses[searchProcessUuid]->status();
}

bool QnManualCameraAdditionRestHandler::isSearchActive(const QnUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);
    return m_searchProcesses.contains(searchProcessUuid);
}

