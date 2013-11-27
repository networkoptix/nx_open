#include "ptz_handler.h"

#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "utils/common/util.h"
#include "utils/common/json.h"
#include "utils/math/space_mapper.h"
#include "core/resource/network_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/camera_resource.h"

static const int OLD_SEQUENCE_THRESHOLD = 1000 * 60 * 5;

namespace {
    QN_DEFINE_NAME_MAPPED_ENUM(PtzAction,
        ((PtzContinousMoveAction, "continuousMove"))
    );

} // anonymous namespace

enum PtzAction {
    PtzContinousMoveAction,
    PtzAbsoluteMoveAction,
    PtzRelativeMoveAction,
    PtzGetPositionAction,
};

QnPtzHandler::QnPtzHandler() {
    m_actionNameMapper = createEnumNameMapper<PtzAction>();
}

void QnPtzHandler::cleanupOldSequence()
{
    QMap<QString, SequenceInfo>::iterator itr = m_sequencedRequests.begin();
    while ( itr != m_sequencedRequests.end())
    {
        SequenceInfo& info = itr.value();
        if (info.m_timer.elapsed() > OLD_SEQUENCE_THRESHOLD)
            itr = m_sequencedRequests.erase(itr);
        else
            ++itr;
    }
}

bool QnPtzHandler::checkSequence(const QString& id, int sequence)
{
    QMutexLocker lock(&m_sequenceMutex);
    cleanupOldSequence();
    if (id.isEmpty())
        return true; // do not check if empty

    if (m_sequencedRequests[id].sequence > sequence)
        return false;

    m_sequencedRequests[id] = SequenceInfo(sequence);
    return true;
}

int QnPtzHandler::executeGet(const QString &path, const QnRequestParams &params, JsonResult &result) {
    QString localPath = path;
    while(localPath.endsWith('/'))
        localPath.chop(1);
    
    QString actionName = localPath.mid(localPath.lastIndexOf('/') + 1);
    int action = m_actionByName.value(actionName, -1);
    if(action == -1) {
        result.setErrorText(QString("Unknown action '%1'.").arg(actionName));
        return CODE_INVALID_PARAMETER;
    }
    
    QString resourceId = params.value("res_id");
    if(resourceId.isEmpty()) {
        result.setErrorText("Parameter 'res_id' is absent or empty.");
        return CODE_INVALID_PARAMETER;
    }

    QnVirtualCameraResourcePtr camera = qnResPool->getNetResourceByPhysicalId(resourceId).dynamicCast<QnVirtualCameraResource>();
    if(!camera) {
        result.setErrorText(QString("Camera resource '%1' not found.").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    if (camera->getStatus() == QnResource::Offline || camera->getStatus() == QnResource::Unauthorized) {
        result.setErrorText(QString("Camera resource '%1' is not ready yet.").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    QnAbstractPtzControllerPtr ptzController;
    if (ptz == 0) {
        result.setErrorText(QString("PTZ is not supported by camera '%1'").arg(resourceId));
        return CODE_INVALID_PARAMETER;
    }

    switch(action) {
    case PtzContinousMoveAction: {
        QVector3D speed;

    }

    }



    /*QnVirtualCameraResourcePtr resource;

    qreal xSpeed = 0;
    qreal ySpeed = 0;
    qreal zoomSpeed = 0;
    qreal xPos = INT_MAX;
    qreal yPos = INT_MAX;
    qreal zoomPos = INT_MAX;
    QString seqId;
    int seqNum = 0;*/
    
    bool resParamFound = false;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "res_id")
        {
            resParamFound = true;
            res = qSharedPointerDynamicCast<QnVirtualCameraResource> (QnResourcePool::instance()->getNetResourceByPhysicalId(params[i].second));
            if (res) {
                errStr.clear();
                ptz = res->getPtzController();
                if (res->getStatus() == QnResource::Offline || res->getStatus() == QnResource::Unauthorized) {
                    errStr = QString("Resource %1 is not ready yet").arg(params[i].second);
                }
                else
                {
                    if (ptz == 0) {
                        errStr = QString("PTZ control not supported by resource %1").arg(params[i].second);
                    }
                }
            }
            else {
                errStr = QString("Camera resource %1 not found").arg(params[i].second);
            }
        }
        else if (params[i].first == "xSpeed")
            xSpeed = params[i].second.toDouble();
        else if (params[i].first == "ySpeed")
            ySpeed = params[i].second.toDouble();
        else if (params[i].first == "zoomSpeed")
            zoomSpeed = params[i].second.toDouble();
        else if (params[i].first == "xPos")
            xPos = params[i].second.toDouble();
        else if (params[i].first == "yPos")
            yPos = params[i].second.toDouble();
        else if (params[i].first == "zoomPos")
            zoomPos = params[i].second.toDouble();
        else if (params[i].first == "seqId")
            seqId = params[i].second;
        else if (params[i].first == "seqNum")
            seqNum = params[i].second.toInt();
    }
    if (!resParamFound)
        errStr = QLatin1String("parameter 'res_id' is absent");
    else if (action.isEmpty())
    {
        errStr = QLatin1String("PTZ action is not defined. Use api/ptz/<action> in HTTP path");
    }

    if (res)
    {
        // ignore other errors (resource status for example)
        if (action == "calibrate") 
        {
            if (QnAbstractPtzController::calibrate(res, xSpeed, ySpeed, zoomSpeed))
            {
                result.append("<root>\n");
                result.append("SUCCESS");
                result.append("</root>\n");
                return CODE_OK;
            }
            else {
                errStr = "Can not save PTZ parameters to database";
            }
        }
        else if (action == "getCalibrate") {
            QnAbstractPtzController::getCalibrate(res, xSpeed, ySpeed, zoomSpeed);
            result.append("<root>\n");
            result.append("<velocity>\n");
            QString velocity("<velocity xSpeed=\"%1\" ySpeed=\"%2\" zoomSpeed=\"%3\" />\n");
            result.append(QString("<xSpeed>%1</xSpeed>").arg(xSpeed).toUtf8());
            result.append(QString("<ySpeed>%1</ySpeed>").arg(ySpeed).toUtf8());
            result.append(QString("<zoomSpeed>%1</zoomSpeed>").arg(zoomSpeed).toUtf8());
            result.append("</velocity>\n");
            result.append("</root>\n");
            return CODE_OK;
        }
    }


    if (!errStr.isEmpty()) {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }



    if (action == "move")
    {
        if (checkSequence(seqId, seqNum)) {
            if (ptz->startMove(xSpeed, ySpeed, zoomSpeed) != 0)
                errStr = "Error executing PTZ command";
        }
    }
    else if (action == "moveTo")
    {
        if (xPos == INT_MAX || yPos == INT_MAX) {
            errStr = "Paremeters 'xPos', 'yPos' MUST be provided\n";
        }
        else if (checkSequence(seqId, seqNum)) {
            if (ptz->moveTo(xPos, yPos, zoomPos) != 0)
                errStr = "Error executing PTZ command";
        }
    }
    else if (action == "getPosition")
    {
        if (ptz->getPosition(&xPos, &yPos, &zoomPos) != 0) {
            errStr = "Error executing PTZ command";
        }
        else {
            result.append("<root>\n");
            result.append("<position>\n");
            result.append(QString("<xPos>%1</xPos>").arg(xPos).toUtf8());
            result.append(QString("<yPos>%1</yPos>").arg(yPos).toUtf8());
            result.append(QString("<zoomPos>%1</zoomPos>").arg(zoomPos).toUtf8());
            result.append("</position>\n");
            result.append("</root>\n");
            return CODE_OK;
        }
    }
    else if (action == "stop")
    {
        if (checkSequence(seqId, seqNum)) {
            if (ptz->stopMove() != 0)
                errStr = "Error executing PTZ command";
        }
    }
    else if (action == "getSpaceMapper")
    {
        QnAbstractPtzController *ptzController = res->getPtzController();
        const QnPtzSpaceMapper *spaceMapper = ptzController ? ptzController->getSpaceMapper() : NULL;
        if(spaceMapper) {
            QJsonObject map;
            QJson::serialize(*spaceMapper, "reply", &map);
            QJson::serialize(map, &result);
        } else {
            result = "{ \"reply\": null }";
        }
        contentType = "application/json";
    }
    else 
    {
        errStr = QByteArray("Unknown ptz command ").append(action.toUtf8()).append("\n");
    } 

    if(contentType != "application/json") { // TODO: hack!
        result.append("<root>\n");
        if (errStr.isEmpty())
            result.append("SUCCESS");
        else
            result.append(errStr);
        result.append("</root>\n");
    }

    return errStr.isEmpty() ? CODE_OK : CODE_INVALID_PARAMETER;

}

int QnPtzHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnPtzHandler::description() const
{
    return "\
        There are several ptz commands: <BR>\
        <b>api/ptz/move</b> - start camera moving.<BR>\
        <b>api/ptz/moveTo</b> - go to absolute position.<BR>\
        <b>api/ptz/stop</b> - stop camera moving.<BR>\
        <b>api/ptz/getPosition</b> - return current camera position.<BR>\
        <b>api/ptz/getSpaceMapper</b> - return JSON-serialized PTZ space mapper for the given camera, if any.<BR>\
        <b>api/ptz/calibrate</b> - calibrate moving speed (addition speed coeff).<BR>\
        <b>api/ptz/getCalibrate</b> - read current calibration settings.<BR>\
        <BR>\
        Param <b>res_id</b> - camera physicalID.<BR>\
        <BR>\
        Arguments for 'move' and 'calibrate' commands:<BR>\
        Param <b>xSpeed</b> - rotation X velocity in range [-1..+1].<BR>\
        Param <b>ySpeed</b> - rotation Y velocity in range [-1..+1].<BR>\
        Param <b>zoomSpeed</b> - zoom velocity in range [-1..+1].<BR>\
        <BR>\
        Arguments for 'moveTo' commands:<BR>\
        Param <b>xPos</b> - go to absolute X position in range [-1..+1].<BR>\
        Param <b>yPos</b> - go to absolute Y position in range [-1..+1].<BR>\
        Param <b>zoomPos</b> - Optional. Go to absolute zoom position in range [0..+1].<BR>\
        <BR>\
        If PTZ command do not return data, function return simple 'OK' message on success or error message if command fail. \
        For 'getCalibrate' command returns XML with coeffecients. For 'getPosition' command returns XML with current position.\
    ";
}
