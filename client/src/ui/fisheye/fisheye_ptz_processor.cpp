#include "fisheye_ptz_processor.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <math.h>
#include "utils/math/space_mapper.h"

namespace {
    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovRad) {
        return (36.0 / 2.0) / std::tan((fovRad/ 2.0));
    }

    qreal mm35vToFov(qreal mm) {
        return std::atan((36.0 / 2.0) / mm) * 2.0;
    }

} // anonymous namespace

#define toRadian(x) (x * M_PI / 180.0)
#define toGrad(x) (x * 180.0 / M_PI)

qreal MAX_MOVE_SPEED = 1.0; // 1 rad per second
qreal MAX_ZOOM_SPEED = toRadian(30.0); // zoom speed
qreal MIN_FOV = toRadian(30.0);
qreal MAX_FOV = toRadian(90.0);

qreal FISHEYE_FOV = toRadian(180.0);

QnFisheyePtzController::QnFisheyePtzController(QnResource* resource):
    QnAbstractPtzController(resource),
    m_renderer(0),
    m_lastTime(0)
{
    QnScalarSpaceMapper xMapper(-90.0, 90.0, -90.0, 90.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper yMapper(-90.0, 90.0, -90.0, 90.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper zMapper(fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), fovTo35mmEquiv(MIN_FOV), fovTo35mmEquiv(MAX_FOV), Qn::ConstantExtrapolation);

    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());
}

QnFisheyePtzController::~QnFisheyePtzController()
{
    if (m_renderer)
        m_renderer->setFisheyeController(0);
    delete m_spaceMapper;
}

void QnFisheyePtzController::addRenderer(QnResourceWidgetRenderer* renderer)
{
    m_renderer = renderer;
    m_renderer->setFisheyeController(this);
}

int QnFisheyePtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity)
{
    m_motion = QVector3D(xVelocity, yVelocity, zoomVelocity);
    m_lastTime = getUsecTimer();

    return 0;
}

int QnFisheyePtzController::stopMove()
{
    m_motion = QVector3D();
    return 0;
}

int QnFisheyePtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos)
{
    m_motion = QVector3D();
    
    m_devorpingParams.fov = qBound(MIN_FOV, mm35vToFov(zoomPos), MAX_FOV);

    qreal xRange = (FISHEYE_FOV - m_devorpingParams.fov) / 2.0;
    qreal yRange = xRange / m_devorpingParams.aspectRatio;

    m_devorpingParams.xAngle = qBound(-xRange, toRadian(xPos), xRange);
    m_devorpingParams.yAngle = qBound(-yRange, toRadian(yPos), yRange);

    return 0;
}

int QnFisheyePtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos)
{
    *xPos = toGrad(m_devorpingParams.xAngle);
    *yPos = toGrad(m_devorpingParams.yAngle);
    *zoomPos = fovTo35mmEquiv(m_devorpingParams.fov);

    return 0;
}


Qn::PtzCapabilities QnFisheyePtzController::getCapabilities()
{
    return Qn::AllPtzCapabilities;
}

const QnPtzSpaceMapper* QnFisheyePtzController::getSpaceMapper()
{
    return m_spaceMapper;
}


void QnFisheyePtzController::at_timer()
{

}

void QnFisheyePtzController::setAspectRatio(float aspectRatio)
{
    m_devorpingParams.aspectRatio = aspectRatio;
}

DevorpingParams QnFisheyePtzController::getDevorpingParams()
{
    qint64 newTime = getUsecTimer();

    qreal zoomSpeed = -m_motion.z() * MAX_ZOOM_SPEED;
    qreal xSpeed = m_motion.x() * MAX_MOVE_SPEED;
    qreal ySpeed = m_motion.y() * MAX_MOVE_SPEED;

    qreal timeSpend = (newTime - m_lastTime) / 1000000.0;
    
    m_devorpingParams.fov += zoomSpeed * timeSpend;
    m_devorpingParams.fov = qBound(MIN_FOV, m_devorpingParams.fov, MAX_FOV);

    m_devorpingParams.xAngle += xSpeed * timeSpend;
    m_devorpingParams.yAngle += ySpeed * timeSpend;

    qreal xRange = (FISHEYE_FOV - m_devorpingParams.fov) / 2.0;
    qreal yRange = xRange; // / m_devorpingParams.aspectRatio;

    m_devorpingParams.xAngle = qBound(-xRange, m_devorpingParams.xAngle, xRange);
    m_devorpingParams.yAngle = qBound(-yRange, m_devorpingParams.yAngle, yRange);
    
    m_lastTime = newTime;

    return m_devorpingParams;
}
