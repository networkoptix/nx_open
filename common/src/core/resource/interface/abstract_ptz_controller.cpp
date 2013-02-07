#include "abstract_ptz_controller.h"

#include <utils/common/warnings.h>

#include <api/api_fwd.h>
#include <api/app_server_connection.h>

#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/param.h>


QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr &resource) {
    m_resource = resource.dynamicCast<QnVirtualCameraResource>();
    if(!m_resource)
        qnWarning("Invalid non-camera resource '%1' provided to ptz controller.", resource ? resource->getName() : QLatin1String("NULL"));
}

void QnAbstractPtzController::getCalibrate(QnVirtualCameraResourcePtr res, qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff)
{
    QVariant val = 1.0;
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

    QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
    if (conn->saveSync(res) != 0) 
    {
        qCritical() << "QnPlOnvifResource::init: can't save resource PTZ params to Enterprise Controller. Resource physicalId: "
            << res->getPhysicalId() << ". Error: " << conn->getLastError();
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
