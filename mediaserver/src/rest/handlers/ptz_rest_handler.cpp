#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "utils/common/util.h"
#include "ptz_rest_handler.h"
#include "core/resource/network_resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/camera_resource.h"

QnPtzRestHandler::QnPtzRestHandler()
{

}

int QnPtzRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    QnVirtualCameraResourcePtr res;
    QnAbstractPtzController* ptz = 0;
    QString errStr;

    QString action = path.mid(path.lastIndexOf('/') + 1);
    
    qreal xSpeed = 1.0;
    qreal ySpeed = 1.0;
    qreal zoomSpeed = 1.0;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "res_id")
        {
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
    }
    if (res == 0)
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
            QString velocity("<velocity xSpeed=\"%1\" ySpeed=\"%2\" zoomSpeed=\"%3\" />\n");
            result.append(velocity.arg(xSpeed).arg(ySpeed).arg(zoomSpeed).toUtf8());
            result.append("</root>\n");
            return CODE_OK;
        }
    }


    if (!errStr.isEmpty()) {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_NOT_FOUND;
    }

    if (action == "move")
    {
        ptz->startMove(xSpeed, ySpeed, zoomSpeed);
    }
    else if (action == "stop")
    {
        ptz->stopMove();
    }
    else {
        result.append("<root>\n");
        result.append("Unknown ptz command ").append(action.toUtf8()).append("\n");
        result.append("</root>\n");
    }

    result.append("<root>\n");
    result.append("SUCCESS");
    result.append("</root>\n");

    return CODE_OK;

}

int QnPtzRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnPtzRestHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "There is several ptz command: <BR>";
    rez += "<b>api/ptz/move</b> - start camera moving <BR>";
    rez += "<b>api/ptz/stop</b> - stop camera moving <BR>";
    rez += "<b>api/ptz/calibrate</b> - calibrate moving speed (addition speed coeff) <BR>";
    rez += "<b>api/ptz/getCalibrate</b> - read current calibration settings <BR>";
    rez += "All commands uses same input arguments except of 'getCalibrate' there is not arguments";
    rez += "<BR>Param <b>xSpeed</b> - rorate X velocity in range [-1..+1]";
    rez += "<BR>Param <b>ySpeed</b> - rorate Y velocity in range [-1..+1]";
    rez += "<BR>Param <b>zoom</b> - zoom velocity in range [-1..+1]";
    rez += "<BR>Returns 'OK' if PTZ command complete success. Otherwise returns error message. For getCalibrate command returns XML with coeffecients";
    return rez;
}
