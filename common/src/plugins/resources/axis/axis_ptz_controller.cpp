#include "axis_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/network/simple_http_client.h>

#include "axis_resource.h"

static const quint16 DEFAULT_AXIS_API_PORT = 80; // TODO: #Elric copypasta from axis_resource.cpp


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

        QVariant variant(result); // TODO: #Elric use sane lexical cast here.
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

        QVariant variant(result); // TODO: #Elric use sane lexical cast here.
        if(variant.convert(static_cast<QVariant::Type>(qMetaTypeId<T>()))) {
            *value = variant.value<T>();
            return true;
        } else {
            return false;
        }
    }

private:
#ifdef _DEBUG /* QMap is easier to look through in debug. */
    QMap<QString, QString> m_data;
#else
    QHash<QString, QString> m_data;
#endif
};


QnAxisPtzController::QnAxisPtzController(QnPlAxisResource* resource):
    QnAbstractPtzController(resource),
    m_resource(resource),
    m_capabilities(Qn::NoCapabilities),
    m_spaceMapper(NULL)
{
    QnAxisParameterMap params;
    if(query(lit("param.cgi?action=list"), &params))
        init(params);
}

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

void QnAxisPtzController::init(const QnAxisParameterMap &params) {
    QString ptzEnabled = params.value<QString>("root.Properties.PTZ.PTZ");
    if(ptzEnabled != lit("yes"))
        return;

    if(params.value<bool>("root.PTZ.Various.V1.Locked"))
        return;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousPan") && params.value<bool>("root.PTZ.Support.S1.ContinuousTilt"))
        m_capabilities |= Qn::ContinuousPanTiltCapability;

    if(params.value<bool>("root.PTZ.Support.S1.ContinuousZoom"))
        m_capabilities |= Qn::ContinuousZoomCapability;

    if(params.value<bool>("root.PTZ.Support.S1.AbsolutePan") && params.value<bool>("root.PTZ.Support.S1.AbsoluteTilt") && params.value<bool>("root.PTZ.Support.S1.AbsoluteZoom"))
        m_capabilities |= Qn::AbsolutePtzCapability;

    if(!params.value<bool>("root.PTZ.Various.V1.PanEnabled", true) || !params.value<bool>("root.PTZ.Various.V1.TiltEnabled", true))
        m_capabilities &= ~(Qn::AbsolutePtzCapability | Qn::ContinuousPanTiltCapability);

    if(!params.value<bool>("root.PTZ.Various.V1.ZoomEnabled", true))
        m_capabilities &= ~(Qn::AbsolutePtzCapability | Qn::ContinuousZoomCapability);

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
        maxTilt = qMin(maxTilt, 90.0);
        minTilt = qMax(minTilt, -90.0);

        /* Axis takes care of image rotation automagically, but we still need to adjust tilt limits. */
        qreal rotation = params.value("root.Image.I0.Appearance.Rotation", 0.0);
        if(qFuzzyCompare(rotation, 180.0)) {
            minTilt = -minTilt;
            maxTilt = -maxTilt;
            qSwap(minTilt, maxTilt);
        }

        QnScalarSpaceMapper xMapper(minPan, maxPan, minPan, maxPan, qFuzzyCompare(maxPan - minPan, 360.0) ? Qn::PeriodicExtrapolation : Qn::ConstantExtrapolation);
        QnScalarSpaceMapper yMapper(minTilt, maxTilt, minTilt, maxTilt, Qn::ConstantExtrapolation);
        QnScalarSpaceMapper zMapper(1, 9999, fovTo35mmEquiv(gradToRad(maxAngle)), fovTo35mmEquiv(gradToRad(minAngle)), Qn::ConstantExtrapolation);
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
    QByteArray ptz_ctl_id;
repeat:
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
    } 
    else if (status == CL_HTTP_REDIRECT && ptz_ctl_id.isEmpty())
    {
        ptz_ctl_id = http->header().value("Set-Cookie");
        if (ptz_ctl_id.isEmpty())
            return false;
        http->addHeader("Cookie", ptz_ctl_id);
        status = http->doGET(lit("axis-cgi/%1").arg(request).toLatin1());
        goto repeat;
    }
    else {
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

            QString key = line.left(index);
            QString value = line.mid(index + 1);

            for (int i = 2; i <= 4; ++i) {
                key.replace(lit("root.PTZ.Support.S%1").arg(i), lit("root.PTZ.Support.S1")); // TODO: #Elric evil hardcode, implement properly
                key.replace(lit("root.PTZ.Various.V%1").arg(i), lit("root.PTZ.Various.V1"));
            }

            params->insert(key, value);
        }
    }

    return true;
}

QString QnAxisPtzController::getCameraNum()
{
    QString camNum;
    QVariant val;
    m_resource->getParam(QLatin1String("channelsAmount"), val, QnDomainMemory);
    if (val.toInt() > 1)
        camNum = QString(lit("camera=%1&")).arg(m_resource->getChannelNum());
    return camNum;
}

int QnAxisPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) {
     // TODO: #Elric *90? Just move all logical-physical transformations to mediaserver.
    return !query(lit("com/ptz.cgi?%1continuouspantiltmove=%2,%3&continuouszoommove=%4").arg(getCameraNum()).arg(xVelocity * 90).arg(yVelocity * 90).arg(zoomVelocity));
}

int QnAxisPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) {
    return !query(lit("com/ptz.cgi?%1pan=%2&tilt=%3&zoom=%4").arg(getCameraNum()).arg(xPos).arg(yPos).arg(zoomPos));
}

int QnAxisPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) {
    QnAxisParameterMap params;
    int status = !query(lit("com/ptz.cgi?%1query=position").arg(getCameraNum()), &params);
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

Qn::PtzCapabilities QnAxisPtzController::getCapabilities() {
    return m_capabilities;
}

const QnPtzSpaceMapper *QnAxisPtzController::getSpaceMapper() {
    return m_spaceMapper;
}
