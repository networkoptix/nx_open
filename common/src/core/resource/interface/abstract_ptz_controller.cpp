#include "abstract_ptz_controller.h"

#include "core/resource/network_resource.h"
#include "api/app_server_connection.h"
#include "core/resource/camera_resource.h"
#include "api/api_fwd.h"
#include "core/resource/param.h"

QnAbstractPtzController::QnAbstractPtzController(QnResourcePtr netRes)
{
    m_resource = qSharedPointerDynamicCast<QnVirtualCameraResource>(netRes);

}

void QnAbstractPtzController::getCalibrate(QnVirtualCameraResourcePtr res, qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff)
{
    QVariant val;
    res->getParam(QLatin1String("xVelocityCoeff"), val, QnDomainDatabase);
    xVelocityCoeff = val.toFloat();
    res->getParam(QLatin1String("yVelocityCoeff"), val, QnDomainDatabase);
    yVelocityCoeff = val.toFloat();
    res->getParam(QLatin1String("zoomVelocityCoeff"), val, QnDomainDatabase);
    zoomVelocityCoeff = val.toFloat();
}

void QnAbstractPtzController::getCalibrate(qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff)
{
    getCalibrate(m_resource, xVelocityCoeff, yVelocityCoeff, zoomVelocityCoeff);
}

bool QnAbstractPtzController::calibrate(QnVirtualCameraResourcePtr res, qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff)
{
    res->setParam(QLatin1String("xVelocityCoeff"), xVelocityCoeff, QnDomainDatabase);
    res->setParam(QLatin1String("yVelocityCoeff"), yVelocityCoeff, QnDomainDatabase);
    res->setParam(QLatin1String("zoomVelocityCoeff"), zoomVelocityCoeff, QnDomainDatabase);

    QByteArray errorStr;
    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(res, errorStr) != 0) 
    {
        qCritical() << "QnPlOnvifResource::init: can't save resource PTZ params to Enterprise Controller. Resource physicalId: "
            << res->getPhysicalId() << ". Error: " << errorStr;
        return false;
    }
    return true;
}

bool QnAbstractPtzController::calibrate(qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff)
{
    return calibrate(m_resource, xVelocityCoeff, yVelocityCoeff, zoomVelocityCoeff);
}

qreal QnAbstractPtzController::getXVelocityCoeff() const
{
    QVariant val = 1.0;
    m_resource->getParam(QLatin1String("xVelocityCoeff"), val, QnDomainDatabase);
    return val.toFloat();
}

qreal QnAbstractPtzController::getYVelocityCoeff() const
{
    QVariant val = 1.0;
    m_resource->getParam(QLatin1String("yVelocityCoeff"), val, QnDomainDatabase);
    return val.toFloat();
}

qreal QnAbstractPtzController::getZoomVelocityCoeff() const
{
    QVariant val = 1.0;
    m_resource->getParam(QLatin1String("zoomVelocityCoeff"), val, QnDomainDatabase);
    return val.toFloat();
}
