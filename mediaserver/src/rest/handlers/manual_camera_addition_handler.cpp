#include "manual_camera_addition_handler.h"

#include <QtNetwork/QAuthenticator>
#include <QtConcurrent/QtConcurrentRun>

#include <api/media_server_cameras_data.h>
#include <core/resource_managment/resource_discovery_manager.h>
#include <core/resource/resource.h>
//#include <core/resource_managment/resource_searcher.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <utils/common/json.h>
#include <utils/network/tcp_connection_priv.h>


namespace {
    static const int searchThreadCount = 4;
}

QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

void searchResourcesAsync(QnManualCameraSearcher* searcher, const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port) {
    searcher->run(startAddr, endAddr, auth, port);
}

int QnManualCameraAdditionHandler::searchStartAction(const QnRequestParamList &params,  JsonResult &result)
{
    int port = 0;
    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin"); // default values

    QString addr1;
    QString addr2;

    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "start_ip") 
            addr1 = param.second;
        else if (param.first == "end_ip")
            addr2 = param.second;
        else if (param.first == "user")
            auth.setUser(param.second);
        else if (param.first == "password")
            auth.setPassword(param.second);
        else if (param.first == "port")
            port = param.second.toInt();
    }

    if (addr1.isNull())
        return CODE_INVALID_PARAMETER;

    if (addr2 == addr1)
        addr2.clear();

    QUuid processUuid = QUuid::createUuid();

    QnManualCameraSearcher searcher;
    {
        QMutexLocker lock(&m_searchProcessMutex);
        m_searchProcesses.insert(processUuid, searcher);
    }

    QtConcurrent::run(&searchResourcesAsync, &searcher, addr1, addr2, auth, port);

    QnManualCameraSearchProcessReply reply;
    reply.status = getSearchStatus(processUuid);
    reply.processUuid = processUuid;
    result.setReply(reply);

    return CODE_OK;
}

int QnManualCameraAdditionHandler::searchStatusAction(const QnRequestParamList &params, JsonResult &result) {
    QUuid processUuid;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "uuid")
            processUuid = QUuid(param.second);
    }

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    if (!isSearchActive(processUuid))
        return CODE_NOT_FOUND;

    QnManualCameraSearchProcessReply reply;
    reply.processUuid = processUuid;
    reply.status = getSearchStatus(processUuid);
    reply.cameras = getSearchResults(processUuid);
    result.setReply(reply);

    return CODE_OK;
}


int QnManualCameraAdditionHandler::searchStopAction(const QnRequestParamList &params, JsonResult &result) {
    QUuid processUuid;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "uuid")
            processUuid = QUuid(param.second);
    }

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    if (isSearchActive(processUuid)) {
        QMutexLocker lock(&m_searchProcessMutex);
        m_searchProcesses[processUuid].cancel();
    }

    QnManualCameraSearchProcessReply reply;
    reply.processUuid = processUuid;
    reply.status = getSearchStatus(processUuid);
    reply.cameras = getSearchResults(processUuid);
    result.setReply(reply);

    if (isSearchActive(processUuid)) {
        QMutexLocker lock(&m_searchProcessMutex);
        m_searchProcesses.remove(processUuid);
    }

    return CODE_OK;
}


int QnManualCameraAdditionHandler::addCamerasAction(const QnRequestParamList &params,  JsonResult &result)
{
    QAuthenticator auth;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "user")
            auth.setUser(param.second);
        else if (param.first == "password")
            auth.setPassword(param.second);
    }

    QString resType;
    QUrl url;
    QnManualCamerasMap cameras;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "url") 
        {
            if (!url.isEmpty()) {
                QnManualCamerasMap::iterator itr = cameras.insert(url.toString(), QnManualCameraInfo(url, auth, resType));
                if (itr.value().resType == 0)
                    return CODE_INVALID_PARAMETER;
            }
            url = param.second;
        }
        else if (param.first == "manufacturer")
            resType = param.second;
    }

    if (!url.isEmpty()) {
        QnManualCamerasMap::iterator itr = cameras.insert(url.toString(), QnManualCameraInfo(url, auth, resType));
        if (itr.value().resType == 0)
            return CODE_INVALID_PARAMETER;
    }
    
    bool registered = QnResourceDiscoveryManager::instance()->registerManualCameras(cameras);
    result.setReply(registered);
    return registered ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionHandler::executeGet(const QString &path, const QnRequestParamList &params, JsonResult &result) {
    QString localPath = path;
    while(localPath.endsWith('/'))
        localPath.chop(1);
    QString action = localPath.mid(localPath.lastIndexOf('/') + 1);
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

QnManualCameraSearchStatus QnManualCameraAdditionHandler::getSearchStatus(const QUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);

    if (!m_searchProcesses.contains(searchProcessUuid))
        return QnManualCameraSearchStatus();

    return m_searchProcesses[searchProcessUuid].status();
}

QnManualCameraSearchCameraList QnManualCameraAdditionHandler::getSearchResults(const QUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);

    if (!m_searchProcesses.contains(searchProcessUuid))
        return QnManualCameraSearchCameraList();

    return m_searchProcesses[searchProcessUuid].results();
}

bool QnManualCameraAdditionHandler::isSearchActive(const QUuid &searchProcessUuid) {
    QMutexLocker lock(&m_searchProcessMutex);
    return m_searchProcesses.contains(searchProcessUuid);
}

QString QnManualCameraAdditionHandler::description() const
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
