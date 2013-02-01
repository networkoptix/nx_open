#include "axis_ptz_controller.h"

#include <utils/common/math.h>
#include <utils/common/space_mapper.h>
#include <utils/network/simple_http_client.h>

#include "axis_resource.h"

static const quint16 DEFAULT_AXIS_API_PORT = 80; // TODO: #Elric copypasta from axis_resource.cpp

namespace {
    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovDegreees) {
        return (36.0 / 2) / std::tan((fovDegreees / 2) * (M_PI / 180));
    }

} // anonymous namespace


class QnAxisParameterMap {
public:
    QnAxisParameterMap() {};

    void insert(const QString &key, const QString &value) {
        m_data[key] = value;
    }

    template<class T>
    T value(const char *key, const T &defaultValue = T()) const {
        QString result = m_data.value(QLatin1String(key));
        if(result.isNull())
            return defaultValue;

        QVariant variant(result); // TODO: use sane lexical cast here.
        if(variant.convert(static_cast<QVariant::Type>(qMetaTypeId<T>()))) {
            return variant.value<T>();
        } else {
            return defaultValue;
        }
    }

    template<class T>
    bool value(const char *key, T *value) const {
        QString result = m_data.value(QLatin1String(key));
        if(result.isNull())
            return false;

        QVariant variant(result); // TODO: use sane lexical cast here.
        if(variant.convert(static_cast<QVariant::Type>(qMetaTypeId<T>()))) {
            *value = variant.value<T>();
            return true;
        } else {
            return false;
        }
    }

private:
    QHash<QString, QString> m_data;
};


QnAxisPtzController::QnAxisPtzController(const QnPlAxisResourcePtr &resource, QObject *parent):
    QnAbstractPtzController(resource, parent),
    m_resource(resource),
    m_capabilities(0),
    m_spaceMapper(NULL)
{
    QnAxisParameterMap params;
    if(query(lit("param.cgi?action=list&group=PTZ"), &params))
        init(params);
}

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

void QnAxisPtzController::init(const QnAxisParameterMap &params) {
    if(params.value<bool>("root.PTZ.Support.S1.ContinuousPan") && params.value<bool>("root.PTZ.Support.S1.ContinuousTilt"))
        m_capabilities |= Qn::ContinuousPanTiltCapability;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousZoom"))
        m_capabilities |= Qn::ContinuousZoomCapability;

    if(params.value<bool>("root.PTZ.Support.S1.AbsolutePan") && params.value<bool>("root.PTZ.Support.S1.AbsoluteTilt") && params.value<bool>("root.PTZ.Support.S1.AbsoluteZoom"))
        m_capabilities |= Qn::AbsolutePtzCapability;
 
    qreal minPan, maxPan, minTilt, maxTilt, minAngle, maxAngle;
    if(
        params.value("root.PTZ.Limit.L1.MinPan", &minPan) &&
        params.value("root.PTZ.Limit.L1.MaxPan", &maxPan) &&
        params.value("root.PTZ.Limit.L1.MinTilt", &minTilt) &&
        params.value("root.PTZ.Limit.L1.MaxTilt", &maxTilt) &&
        params.value("root.PTZ.Limit.L1.MinFieldAngle", &minAngle) &&
        params.value("root.PTZ.Limit.L1.MaxFieldAngle", &maxAngle) &&
        minPan <= maxPan && 
        minTilt <= maxTilt &&
        minAngle <= maxAngle
    ) {
        /* These are in 1/10th of a degree, so we convert them away. */
        minAngle /= 10.0;
        maxAngle /= 10.0;

        /* We implement E-Flip, so we don't want strange tilt angles. */
        maxTilt = qMin(maxTilt, 180.0);
        minTilt = qMax(minTilt, -180.0);

        QnScalarSpaceMapper xMapper(minPan, maxPan, minPan, maxPan, qFuzzyCompare(maxPan - minPan, 360.0) ? Qn::PeriodicExtrapolation : Qn::ConstantExtrapolation);
        QnScalarSpaceMapper yMapper(minTilt, maxTilt, minTilt, maxTilt, Qn::ConstantExtrapolation);
        QnScalarSpaceMapper zMapper(fovTo35mmEquiv(maxAngle), fovTo35mmEquiv(minAngle), 1, 9999, Qn::ConstantExtrapolation);
        /* Note that we do not care about actual zoom limits on the camera. 
         * It's up to the camera to enforce them. */
        
        m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());
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

int QnAxisPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) {
    return !query(lit("com/ptz.cgi?continuouspantiltmove=%1,%2&continuouszoommove=%3").arg(xVelocity).arg(yVelocity).arg(zoomVelocity));
}

int QnAxisPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) {
    return !query(lit("com/ptz.cgi?pan=%1&tilt=%2&zoom=%3").arg(xPos).arg(yPos).arg(zoomPos));
}

int QnAxisPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) {
    QnAxisParameterMap params;
    int status = !query(lit("com/ptz.cgi?query=position"), &params);
    if(status != 0)
        return status;

    qreal pan, tilt, zoom;
    if(params.value("pan", &pan) && params.value("tilt", &tilt) && params.value("zoom", &zoom)) {
        *xPos = pan;
        *yPos = tilt;
        *zoomPos = zoom;
    } else {
        qnWarning("Failed to get PTZ position from camera %1. Malformed response.", m_resource->getName());
        return 1;
    }

    return status;
}

int QnAxisPtzController::stopMove() {
    return startMove(0.0, 0.0, 0.0);
}

Qn::CameraCapabilities QnAxisPtzController::getCapabilities() {
    return m_capabilities;
}

const QnPtzSpaceMapper *QnAxisPtzController::getSpaceMapper() {
    return m_spaceMapper;
}

