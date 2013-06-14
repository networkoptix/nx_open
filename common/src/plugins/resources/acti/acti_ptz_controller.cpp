#include "acti_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>

#include "acti_resource.h"

static const qreal DIGITAL_ZOOM_COEFF = 5.0;
static const qreal ANALOG_ZOOM = 16.0;

static const QString ENCODER_STR(lit("encoder"));

namespace {
    int sign2(qreal value)
    {
        return value >= 0 ? 1 : -1;
    }

    int sign3(qreal value)
    {
        return value > 0 ? 1 : (value < 0 ? -1 : 0);
    }

    int scaleValue(qreal value, int min, int max)
    {
        if (value == 0)
            return 0;

        return int (value * (max-min) + 0.5*sign2(value)) + sign2(value)*min;
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

} // anonymous namespace


QnActiPtzController::QnActiPtzController(QnActiResource* resource):
    QnAbstractPtzController(resource),
    m_resource(resource),
    m_capabilities(Qn::NoCapabilities),
    m_spaceMapper(NULL),
    m_zoomVelocity(0.0),
    m_moveVelocity(0, 0),
    m_minAngle(0.0),
    m_maxAngle(0.0),
    m_isFliped(false),
    m_isMirrored(false)
{
    init();
}

QnActiPtzController::~QnActiPtzController() {
    return;
}

qreal toLogicalScale(qreal src, qreal rangeMin, qreal rangeMax)
{
    return src/1000.0 * (rangeMax-rangeMin) + rangeMin;
}

void QnActiPtzController::init() 
{
    CLHttpStatus status;
    QByteArray zoomString = m_resource->makeActiRequest(ENCODER_STR, lit("ZOOM_CAP_GET"), status, true);
    if (status != CL_HTTP_SUCCESS || !zoomString.startsWith("ZOOM_CAP_GET="))
        return;

    QByteArray flipMode = m_resource->makeActiRequest(ENCODER_STR, lit("VIDEO_FLIP_MODE"), status);
    m_isFliped = flipMode.toInt() == 1;

    QByteArray mirrorMode = m_resource->makeActiRequest(ENCODER_STR, lit("VIDEO_MIRROR_MODE"), status);
    m_isMirrored = mirrorMode.toInt() == 1;

    m_capabilities |= Qn::AbsolutePtzCapability;
    m_capabilities |= Qn::ContinuousPanTiltCapability;
    m_capabilities |= Qn::ContinuousZoomCapability;

    qreal minPanLogical = -17500, maxPanLogical = 17500; // todo: move to camera XML
    qreal minPanPhysical = 360, maxPanPhysical = 0; // todo: move to camera XML
    qreal minTiltLogical = 0, maxTiltLogical = 9000; //  // todo: move to camera XML
    
    qreal minTiltPhysical = -90, maxTiltPhysical = 0; //  // todo: move to camera XML
    if (!m_isFliped) {
        qSwap(minTiltPhysical, maxTiltPhysical);
        m_capabilities &= ~Qn::AbsolutePtzCapability; // acti 8111 has bug for absolute position if flip turned off
    }

    QList<QByteArray> zoomParams = zoomString.split('=')[1].split(',');
    m_minAngle = m_resource->unquoteStr(zoomParams[0]).toInt();
    if (zoomParams.size() > 1)
        m_maxAngle = m_resource->unquoteStr(zoomParams[1]).toInt();

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
}

int QnActiPtzController::stopZoomInternal()
{
    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(ENCODER_STR, lit("ZOOM=STOP"), status);
    int result = (status == CL_HTTP_SUCCESS ? 0 : -1);
    if (result == 0)
        m_zoomVelocity = 0.0;
    return result;
}

int QnActiPtzController::stopMoveInternal()
{
    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(ENCODER_STR, lit("MOVE=STOP"), status);
    int result = (status == CL_HTTP_SUCCESS ? 0 : -1);
    if (result == 0)
        m_moveVelocity = QPair<int, int>(0, 0);
    return result;
}

int QnActiPtzController::startZoomInternal(qreal zoomVelocity)
{
    if (m_zoomVelocity == zoomVelocity)
        return 0;

    stopZoomInternal();

    QString direction = zoomVelocity >= 0 ? lit("TELE") : lit("WIDE");
    int zoomVelocityI = qAbs(scaleValue(zoomVelocity, 2, 7));

    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(ENCODER_STR, QString(lit("ZOOM=%1,%2")).arg(direction).arg(zoomVelocityI), status);
    int result = (status == CL_HTTP_SUCCESS ? 0 : -1);
    
    if (result == 0)
        m_zoomVelocity = zoomVelocity;

    return result;
}

int QnActiPtzController::startMoveInternal(qreal xVelocityR, qreal yVelocityR)
{
    stopMoveInternal();

    if (!m_isFliped) {
        yVelocityR *= -1;
    }


    int xVelocity = qRound(xVelocityR*5.0);
    int yVelocity = qRound(yVelocityR*5.0);

    if (m_moveVelocity.first == xVelocity && m_moveVelocity.second == yVelocity)
        return 0;

    static const QString directions[3][3] =
    {
        { lit("UPLEFT"),   lit("UP"),   lit("UPRIGHT") },
        { lit("LEFT"),     lit("STOP"), lit("RIGHT") },
        { lit("DOWNLEFT"), lit("DOWN"), lit("DOWNRIGHT") }
    };

    QString direction = directions[1-sign3(yVelocity)][sign3(xVelocity)+1];
    QString requestStr = QString(lit("MOVE=%1")).arg(direction);

    if (xVelocity)
        requestStr += QString(lit(",%1")).arg(qAbs(xVelocity));
    if (yVelocity)
        requestStr += QString(lit(",%1")).arg(qAbs(yVelocity));

    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(ENCODER_STR, requestStr, status);
    int result =  (status == CL_HTTP_SUCCESS ? 0 : -1);

    if (result == 0)
        m_moveVelocity = QPair<int, int>(xVelocity, yVelocity);

    return result;
}

int QnActiPtzController::stopMove()
{
    QMutexLocker lock(&m_mutex);

    int errCode1 = 0, errCode2 = 0;
    errCode1 = stopZoomInternal();
    errCode2 = stopMoveInternal();

    return errCode1 ? errCode1 : (errCode2 ? errCode2 : 0);
}

int QnActiPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) 
{
    QMutexLocker lock(&m_mutex);

    int errCode1 = 0, errCode2 = 0;

    if (zoomVelocity) 
        errCode1 = startZoomInternal(zoomVelocity);
    if (xVelocity || yVelocity) 
        errCode2 = startMoveInternal(xVelocity, yVelocity);

    return errCode1 ? errCode1 : (errCode2 ? errCode2 : 0);
}

int QnActiPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) 
{
    QMutexLocker lock(&m_mutex);

    zoomPos = qMax(0.0, (zoomPos-m_minAngle)/(m_maxAngle-m_minAngle) * 1000);

    CLHttpStatus status;

    QByteArray result = m_resource->makeActiRequest(ENCODER_STR, lit("POSITION=ABSOLUTE,%1,%2,5,5").arg(int(xPos)).arg(int(yPos)), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;

    result = m_resource->makeActiRequest(ENCODER_STR, lit("ZOOM=DIRECT,%1").arg(zoomPos), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;

    return 0;
}

int QnActiPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) 
{
    QMutexLocker lock(&m_mutex);

    CLHttpStatus status;

    QByteArray result = m_resource->makeActiRequest(ENCODER_STR, lit("POSITION_GET"), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;
    QList<QByteArray> params = result.split(',');
    if (params.size() != 2)
        return -2;
    *xPos = params[0].toInt();
    *yPos = params[1].toInt();

    result = m_resource->makeActiRequest(ENCODER_STR, lit("ZOOM_POSITION"), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;
    
    *zoomPos = result.toInt();

    return 0;
}

Qn::CameraCapabilities QnActiPtzController::getCapabilities() {
    return m_capabilities;
}

const QnPtzSpaceMapper *QnActiPtzController::getSpaceMapper() {
    return m_spaceMapper;
}
