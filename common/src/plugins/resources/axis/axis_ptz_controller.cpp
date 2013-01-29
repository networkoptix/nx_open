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
    QnAxisParameterMap();

    void insert(const QString &key, const QString &value) {
        m_data[key] = value;
    }

    template<class T>
    T value(const char *key, const T &defaultValue = T()) {
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
    bool value(const char *key, T *value) {
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
    m_capabilities(Qn::ContinuousPtzCapability), // TODO: #Elric need to check for this?
    m_spaceMapper(NULL)
{
    QnAxisParameterMap params = queryParameters(lit("PTZ"));

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

QnAxisPtzController::~QnAxisPtzController() {
    return;
}

CLSimpleHTTPClient *QnAxisPtzController::newHttpClient() const {
    return new CLSimpleHTTPClient(
        m_resource->getHostAddress(), 
        QUrl(m_resource->getUrl()).port(DEFAULT_AXIS_API_PORT), 
        m_resource->getNetworkTimeout(), 
        m_resource->getAuth()
    );
}

QnAxisParameterMap QnAxisPtzController::queryParameters(const QString &group) const {
    QnAxisParameterMap result;
    QScopedPointer<CLSimpleHTTPClient> http(newHttpClient());

    CLHttpStatus status = http->doGET(lit("axis-cgi/param.cgi?action=list&group=%1").arg(group).toLatin1());
    if(status == CL_HTTP_SUCCESS) {
        QByteArray body;
        http->readAll(body);

        if(body.startsWith("Error:")) {
            qnWarning("Failed to read parameter group %1 of camera %2. Camera returned: %3.", group, m_resource->getName(), body.mid(6));
            return result;
        }

        QTextStream stream(&body, QIODevice::ReadOnly);
        while(true) {
            QString line = stream.readLine();
            if(line.isNull())
                break;

            int index = line.indexOf(lit('='));
            if(index == -1)
                continue;

            result.insert(line.left(index), line.mid(index + 1));
        }

        return result;
    } else {
        qnWarning("Failed to read parameter group %1 of camera %2. Result: %3.", group, m_resource->getName(), ::toString(status));
        return result;
    }
}

int QnAxisPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) {
    return 0;
}

int QnAxisPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) {
    return 0;
}

int QnAxisPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) {
    return 0;
}

int QnAxisPtzController::stopMove() {
    return 0;
}

Qn::CameraCapabilities QnAxisPtzController::getCapabilities() {
    return m_capabilities;
}

const QnPtzSpaceMapper *QnAxisPtzController::getSpaceMapper() {
    return NULL;
}