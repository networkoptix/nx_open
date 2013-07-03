#include "fisheye_ptz_processor.h"
#include "ui/graphics/items/resource/resource_widget_renderer.h"
#include <math.h>
#include "utils/math/space_mapper.h"

#define toRadian(x) (x * M_PI / 180.0)

float MAX_MOVE_SPEED = 1.0; // 1 rad per second
float MAX_ZOOM_SPEED = toRadian(30.0); // zoom speed
float MIN_FOV = toRadian(30.0);
float MAX_FOV = toRadian(90.0);

float FISHEYE_FOV = toRadian(180.0);

QnFisheyePtzController::QnFisheyePtzController(QnResource* resource):
    QnAbstractPtzController(resource),
    m_renderer(0),
    m_lastTime(0)
{
    QnScalarSpaceMapper xMapper(-1.0, 1.0, -1.0, 1.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper yMapper(-1.0, 1.0, -1.0, 1.0, Qn::ConstantExtrapolation);
    QnScalarSpaceMapper zMapper(-1.0, 1.0, -1.0, 1.0, Qn::ConstantExtrapolation);

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
    return 0;
}

int QnFisheyePtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos)
{
    *xPos = m_devorpingParams.xAngle;
    *yPos = m_devorpingParams.yAngle;
    *zoomPos = m_devorpingParams.fov;

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

    float zoomSpeed = -m_motion.z() * MAX_ZOOM_SPEED;
    float xSpeed = m_motion.x() * MAX_MOVE_SPEED;
    float ySpeed = -m_motion.y() * MAX_MOVE_SPEED;

    float timeSpend = (newTime - m_lastTime) / 1000000.0;
    
    m_devorpingParams.fov += zoomSpeed * timeSpend;
    m_devorpingParams.fov = qBound(MIN_FOV, m_devorpingParams.fov, MAX_FOV);

    m_devorpingParams.xAngle += xSpeed * timeSpend;
    m_devorpingParams.yAngle += ySpeed * timeSpend;

    float xRange = (FISHEYE_FOV - m_devorpingParams.fov) / 2.0;
    float yRange = xRange / m_devorpingParams.aspectRatio;

    m_devorpingParams.xAngle = qBound(-xRange, m_devorpingParams.xAngle, xRange);
    m_devorpingParams.yAngle = qBound(-yRange, m_devorpingParams.yAngle, yRange);
    
    m_lastTime = newTime;

    return m_devorpingParams;
}
