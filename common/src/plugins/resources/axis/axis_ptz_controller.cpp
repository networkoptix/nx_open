#include "axis_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/network/simple_http_client.h>

#include "axis_resource.h"

static const quint16 DEFAULT_AXIS_API_PORT = 80; // TODO: #Elric copypasta from axis_resource.cpp


// -------------------------------------------------------------------------- //
// QnAxisParameterMap
// -------------------------------------------------------------------------- //
class QnAxisParameterMap {
public:
    QnAxisParameterMap() {};

    void insert(const QString &key, const QString &value) {
        m_data[key] = value;
    }

    template<class T>
    T value(const char *key, const T &defaultValue = T()) const { // TODO: #Elric use QLatin1Literal
        QString result = m_data.value(QLatin1String(key));
        if(result.isNull())
            return defaultValue;

        T value;
        if(QnLexical::deserialize(result, &value)) {
            return value;
        } else {
            return defaultValue;
        }
    }

    template<class T>
    bool value(const char *key, T *value) const { // TODO: #Elric use QLatin1Literal
        QString result = m_data.value(QLatin1String(key));
        if(result.isNull())
            return false;

        return QnLexical::deserialize(result, value);
    }

private:
#ifdef _DEBUG /* QMap is easier to look through in debug. */
    QMap<QString, QString> m_data;
#else
    QHash<QString, QString> m_data;
#endif
};


// -------------------------------------------------------------------------- //
// QnAxisPtzController
// -------------------------------------------------------------------------- //
QnAxisPtzController::QnAxisPtzController(const QnPlAxisResourcePtr &resource):
    QnAbstractPtzController(resource),
    m_resource(resource),
    m_capabilities(Qn::NoCapabilities)
{
    updateState();
}

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

void QnAxisPtzController::updateState() {
    QnAxisParameterMap params;
    if(!query(lit("param.cgi?action=list"), &params))
        return;
    
    updateState(params);
}

void QnAxisPtzController::updateState(const QnAxisParameterMap &params) {
    m_capabilities = 0;

    QString ptzEnabled = params.value<QString>("root.Properties.PTZ.PTZ");
    if(ptzEnabled != lit("yes"))
        return;

    if(params.value<bool>("root.PTZ.Various.V1.Locked", false))
        return;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousPan", false))
        m_capabilities |= Qn::ContinuousPanCapability;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousTilt", false))
        m_capabilities |= Qn::ContinuousTiltCapability;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousZoom", false))
        m_capabilities |= Qn::ContinuousZoomCapability;

    if(params.value<bool>("root.PTZ.Support.S1.AbsolutePan", false))
        m_capabilities |= Qn::AbsolutePanCapability;
        
    if(params.value<bool>("root.PTZ.Support.S1.AbsoluteTilt", false))
        m_capabilities |= Qn::AbsoluteTiltCapability;
        
    if(params.value<bool>("root.PTZ.Support.S1.AbsoluteZoom", false))
        m_capabilities |= Qn::AbsoluteZoomCapability;

    if(!params.value<bool>("root.PTZ.Various.V1.PanEnabled", true))
        m_capabilities &= ~(Qn::ContinuousPanCapability | Qn::AbsolutePanCapability);
        
    if(!params.value<bool>("root.PTZ.Various.V1.TiltEnabled", true))
        m_capabilities &= ~(Qn::ContinuousTiltCapability | Qn::AbsoluteTiltCapability);

    if(!params.value<bool>("root.PTZ.Various.V1.ZoomEnabled", true))
        m_capabilities &= ~(Qn::ContinuousZoomCapability | Qn::AbsoluteZoomCapability);

    /* Note that during continuous move axis takes care of image rotation automagically. */
    qreal rotation = params.value("root.Image.I0.Appearance.Rotation", 0.0);
    if(qFuzzyCompare(rotation, static_cast<qreal>(180.0)))
        m_flip = Qt::Vertical | Qt::Horizontal;

    QnPtzLimits limits;
    if(
        params.value("root.PTZ.Limit.L1.MinPan", &limits.minPan) &&
        params.value("root.PTZ.Limit.L1.MaxPan", &limits.maxPan) &&
        params.value("root.PTZ.Limit.L1.MinTilt", &limits.minTilt) &&
        params.value("root.PTZ.Limit.L1.MaxTilt", &limits.maxTilt) &&
        params.value("root.PTZ.Limit.L1.MinFieldAngle", &limits.minFov) &&
        params.value("root.PTZ.Limit.L1.MaxFieldAngle", &limits.maxFov) &&
        limits.minPan <= limits.maxPan && 
        limits.minTilt <= limits.maxTilt &&
        limits.minFov <= limits.maxFov
    ) {
        /* These are in 1/10th of a degree, so we convert them away. */
        limits.minFov /= 10.0;
        limits.maxFov /= 10.0;

        /* We implement E-Flip, so we don't want strange tilt angles. */
        limits.maxTilt = qMin<qreal>(limits.maxTilt, 90.0);
        limits.minTilt = qMax<qreal>(limits.minTilt, -90.0);

        /* Logical space should work now that we know logical limits. */
        m_capabilities |= Qn::LogicalPositionSpaceCapability;
        m_limits = limits;
        m_logicalToCameraZoom = QnLinearFunction(limits.minFov, 9999, limits.maxFov, 1);
        m_cameraToLogicalZoom = m_logicalToCameraZoom.inversed();
    }
}

CLSimpleHTTPClient *QnAxisPtzController::newHttpClient() const {
    return new CLSimpleHTTPClient(
        m_resource->getHostAddress(), 
        QUrl(m_resource->getUrl()).port(DEFAULT_AXIS_API_PORT), 
        m_resource->getNetworkTimeout(), 
        m_resource->getAuth()
    );
}

bool QnAxisPtzController::query(const QString &request, QByteArray *body) const {
    QScopedPointer<CLSimpleHTTPClient> http(newHttpClient());

    CLHttpStatus status = http->doGET(lit("axis-cgi/%1").arg(request).toLatin1());
    if(status == CL_HTTP_SUCCESS) {
        if(body) {
            QByteArray localBody;
            http->readAll(localBody);
            if(body)
                *body = localBody;

            if(localBody.startsWith("Error:")) {
                qnWarning("Failed to execute request '%1' for camera %2. Camera returned: %3.", request, m_resource->getName(), localBody.mid(6));
                return false;
            }
        }
    } else {
        qnWarning("Failed to execute request '%1' for camera %2. Result: %3.", request, m_resource->getName(), ::toString(status));
        return false;
    }

    return true;
}

bool QnAxisPtzController::query(const QString &request, QnAxisParameterMap *params) const {
    QByteArray body;
    if(!query(request, &body))
        return false;

    if(params) {
        QTextStream stream(&body, QIODevice::ReadOnly);
        while(true) {
            QString line = stream.readLine();
            if(line.isNull())
                break;

            int index = line.indexOf(lit('='));
            if(index == -1)
                continue;

            params->insert(line.left(index), line.mid(index + 1));
        }
    }

    return true;
}

bool QnAxisPtzController::continuousMove(const QVector3D &speed) {
    return query(lit("com/ptz.cgi?continuouspantiltmove=%1,%2&continuouszoommove=%3").arg(speed.x() * 90).arg(speed.y() * 90).arg(speed.z()));
}

bool QnAxisPtzController::getFlip(Qt::Orientations *flip) {
    *flip = m_flip;
    return true;
}

bool QnAxisPtzController::absoluteMove(const QVector3D &position) {
    return query(lit("com/ptz.cgi?pan=%1&tilt=%2&zoom=%3").arg(position.x()).arg(position.y()).arg(m_logicalToCameraZoom(position.z())));
}

bool QnAxisPtzController::getPosition(QVector3D *position) {
    QnAxisParameterMap params;
    if(!query(lit("com/ptz.cgi?query=position"), &params))
        return false;

    qreal pan, tilt, zoom;
    if(params.value("pan", &pan) && params.value("tilt", &tilt) && params.value("zoom", &zoom)) {
        position->setX(pan);
        position->setY(tilt);
        position->setZ(m_cameraToLogicalZoom(zoom));
        return true;
    } else {
        qnWarning("Failed to get PTZ position from camera %1. Malformed response.", m_resource->getName());
        return false;
    }
}

Qn::PtzCapabilities QnAxisPtzController::getCapabilities() {
    return m_capabilities;
}

bool QnAxisPtzController::getLimits(QnPtzLimits *limits) {
    *limits = m_limits;
    return true;
}

bool QnAxisPtzController::relativeMove(qreal, const QRectF &) {
    return false;
}
