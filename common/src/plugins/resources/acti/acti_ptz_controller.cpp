#ifdef ENABLE_ACTI

#include "acti_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>

#include "acti_resource.h"

static const qreal DIGITAL_ZOOM_COEFF = 5.0;
static const qreal ANALOG_ZOOM = 16.0;

namespace {

    int qSign(qreal value) {
        return value >= 0 ? 1 : -1;
    }

    // ACTi 8111
    qreal pointsData[][2] = {
        {0, 1.0},
        {100, 1.2},
        {200, 1.4},
        {300, 1.66},
        {400, 2.0},
        {500, 2.6},
        {600, 3.46},
        {666, 4.0},
        {700, 4.86},
        {750, 5.3},
        {800, 7.0},
        {850, 8.0},
        {875, 8.3},
        {900, 10.6},
        {937, 11.6},
        {950, 12.13},
        {1000, 16.0},
    };

    qreal toLogicalScale(qreal src, qreal rangeMin, qreal rangeMax) {
        return src/1000.0 * (rangeMax-rangeMin) + rangeMin;
    }

} // anonymous namespace


QnActiPtzController::QnActiPtzController(const QnActiResourcePtr &resource):
    base_type(resource),
    m_resource(resource),
    m_isFlipped(false),
    m_isMirrored(false)
{
    init();
    
    panTiltDirection(0, 0); /* Init function-local static. */
}

QnActiPtzController::~QnActiPtzController() {
    return;
}

void QnActiPtzController::init() {
    QByteArray flipData;
    if(!query(lit("VIDEO_FLIP_MODE"), &flipData))
        return;
    m_isFlipped = flipData.toInt() == 1;

    QByteArray mirrorData;
    if(!query(lit("VIDEO_MIRROR_MODE"), &mirrorData))
        return;
    m_isMirrored = mirrorData.toInt() == 1;

    QByteArray zoomData;
    if(!query(lit("ZOOM_CAP_GET"), &zoomData, true))
        return;
    if(!zoomData.startsWith("ZOOM_CAP_GET="))
        return;

    m_capabilities = Qn::NoPtzCapabilities;
    if(m_resource->getModel() == lit("KCM3311")) {
        m_capabilities |= Qn::ContinuousZoomCapability;
    } else {
        m_capabilities |= Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities | Qn::DevicePositioningPtzCapability;
    }
    m_capabilities |= Qn::FlipPtzCapability;

#if 0
    qreal minPanLogical = -17500, maxPanLogical = 17500; // todo: move to camera XML
    qreal minPanPhysical = 360, maxPanPhysical = 0; // todo: move to camera XML
    qreal minTiltLogical = 0, maxTiltLogical = 9000; //  // todo: move to camera XML
    
    qreal minTiltPhysical = -90, maxTiltPhysical = 0; //  // todo: move to camera XML
    if (!m_isFlipped) {
        qSwap(minTiltPhysical, maxTiltPhysical);
        m_capabilities &= ~Qn::AbsolutePtzCapabilities; // acti 8111 has bug for absolute position if flip turned off
    }

    if(m_capabilities & Qn::AbsolutePtzCapabilities)
        m_capabilities |= Qn::LogicalPositioningPtzCapability;

    QList<QByteArray> zoomParams = zoomData.split('=')[1].split(',');
    m_minAngle = m_resource->unquoteStr(zoomParams[0]).toInt();
    if (zoomParams.size() > 1)
        m_maxAngle = m_resource->unquoteStr(zoomParams[1]).toInt();
#endif

#if 0
    QnScalarSpaceMapper xMapper(minPanLogical, maxPanLogical, minPanPhysical, maxPanPhysical, Qn::PeriodicExtrapolation);
    QnScalarSpaceMapper yMapper(minTiltLogical, maxTiltLogical, minTiltPhysical, maxTiltPhysical, Qn::ConstantExtrapolation);

    qreal f35Max = m_maxAngle/DIGITAL_ZOOM_COEFF;
    qreal f35Min = f35Max / ANALOG_ZOOM;

    QVector<QPair<qreal, qreal> > data;
    for (uint i = 0; i < sizeof(pointsData)/sizeof(pointsData[0]); ++i)
        data << QPair<qreal, qreal>(toLogicalScale(pointsData[i][0], m_minAngle, m_maxAngle), pointsData[i][1] * f35Min);
    QnScalarSpaceMapper zMapper(data, Qn::ConstantExtrapolation);

    //QnScalarSpaceMapper toCameraZMapper(0.0, 1000.0, f35Min, f35Min*ANALOG_ZOOM, Qn::ConstantExtrapolation);

    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), 
                                         //QnVectorSpaceMapper(xMapper, yMapper, toCameraZMapper),
                                         QStringList());
#endif
}

bool QnActiPtzController::query(const QString &request, QByteArray *body, bool keepAllData) const {
    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(lit("encoder"), request, status, keepAllData);
    if(body)
        *body = data;
    return status == CL_HTTP_SUCCESS;
}

int QnActiPtzController::toDeviceZoomSpeed(qreal zoomSpeed) const {
    zoomSpeed = qBound(-1.0, zoomSpeed, 1.0);
    if(qFuzzyIsNull(zoomSpeed)) {
        return 0;
    } else {
        /* Zoom speed is an int in [2, 7] range. */
        return qMin(7, qFloor(2.0 + 6.0 * qAbs(zoomSpeed))) * qSign(zoomSpeed); 
    }
}

int QnActiPtzController::toDevicePanTiltSpeed(qreal panTiltSpeed) const {
    panTiltSpeed = qBound(-1.0, panTiltSpeed, 1.0);
    if(qFuzzyIsNull(panTiltSpeed)) {
        return 0;
    } else {
        return qMin(5, qFloor(1.0 + 5.0 * qAbs(panTiltSpeed))) * qSign(panTiltSpeed);
    }
}

QString QnActiPtzController::zoomDirection(int deviceZoomSpeed) const {
    if(deviceZoomSpeed < 0) {
        return lit("WIDE");
    } else if(deviceZoomSpeed == 0) {
        return lit("STOP");
    } else {
        return lit("TELE");
    }
}

QString QnActiPtzController::panTiltDirection(int devicePanSpeed, int deviceTiltSpeed) const {
    if(devicePanSpeed < 0) {
        if(deviceTiltSpeed < 0) {
            return lit("DOWNLEFT");
        } else if(deviceTiltSpeed == 0) {
            return lit("LEFT");
        } else {
            return lit("UPLEFT");
        }
    } else if(devicePanSpeed == 0) {
        if(deviceTiltSpeed < 0) {
            return lit("DOWN");
        } else if(deviceTiltSpeed == 0) {
            return lit("STOP");
        } else {
            return lit("UP");
        }
    } else {
        if(deviceTiltSpeed < 0) {
            return lit("DOWNRIGHT");
        } else if(deviceTiltSpeed == 0) {
            return lit("RIGHT");
        } else {
            return lit("UPRIGHT");
        }
    }
}

bool QnActiPtzController::stopZoomInternal() {
    return query(lit("ZOOM=STOP"));
}

bool QnActiPtzController::stopMoveInternal() {
    return query(lit("MOVE=STOP"));
}

bool QnActiPtzController::startZoomInternal(int deviceZoomSpeed) {
    stopZoomInternal(); // TODO: #Elric needed?

    QString request = lit("ZOOM=%1").arg(zoomDirection(deviceZoomSpeed));
    if(deviceZoomSpeed != 0)
        request += lit(",%1").arg(qAbs(deviceZoomSpeed));

    return query(request);
}

bool QnActiPtzController::startMoveInternal(int devicePanSpeed, int deviceTiltSpeed) {
    stopMoveInternal(); // TODO: #Elric needed?

    if (!m_isFlipped) // TODO: #Elric why only flip? Also check for mirror?
        deviceTiltSpeed *= -1;

    QString request = lit("MOVE=%1").arg(panTiltDirection(devicePanSpeed, deviceTiltSpeed));
    if (devicePanSpeed != 0)
        request += lit(",%1").arg(qAbs(devicePanSpeed));
    if (deviceTiltSpeed != 0)
        request += lit(",%1").arg(qAbs(deviceTiltSpeed));

    return query(request);
}

Qn::PtzCapabilities QnActiPtzController::getCapabilities() {
    return m_capabilities;
}

bool QnActiPtzController::continuousMove(const QVector3D &speed) {
    bool status0;
    if (qFuzzyIsNull(speed.z())) {
        status0 = stopZoomInternal();
    } else {
        status0 = startZoomInternal(toDeviceZoomSpeed(speed.z()));
    }

    bool status1;
    if (qFuzzyIsNull(speed.x()) && qFuzzyIsNull(speed.y())) {
        status1 = stopMoveInternal();
    } else {
        status1 = startMoveInternal(toDevicePanTiltSpeed(speed.x()), toDevicePanTiltSpeed(speed.y()));
    }

    return status0 && status1;
}

bool QnActiPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    int devicePanTiltSpeed = toDevicePanTiltSpeed(qBound(0.01, speed, 1.0));
    if(!query(lit("POSITION=ABSOLUTE,%1,%2,%3,%3").arg(int(position.x())).arg(int(position.y())).arg(devicePanTiltSpeed)))
        return false;

    if(!query(lit("ZOOM=DIRECT,%1").arg(position.z())))
        return false;

    return true;
}

bool QnActiPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    QMutexLocker lock(&m_mutex);

    QByteArray positionData;
    if(!query(lit("POSITION_GET"), &positionData))
        return false;

    QByteArray zoomData;
    if(!query(lit("ZOOM_POSITION"), &zoomData))
        return false;

    QList<QByteArray> panTiltData = positionData.split(',');
    if (panTiltData.size() != 2)
        return false;
    
    position->setX(panTiltData[0].toInt());
    position->setY(panTiltData[1].toInt());
    position->setZ(zoomData.toInt());
    return true;
}

bool QnActiPtzController::getFlip(Qt::Orientations *flip) {
    *flip = 0;

    if(m_isFlipped)
        *flip |= Qt::Vertical;
    if(m_isMirrored)
        *flip |= Qt::Horizontal;

    return true;
}

#endif // ENABLE_ACTI
