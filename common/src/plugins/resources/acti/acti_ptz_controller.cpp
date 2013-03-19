#include "acti_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/network/simple_http_client.h>

#include "acti_resource.h"

static const quint16 DEFAULT_ACTI_API_PORT = 80;
static const qreal DIGITAL_ZOOM_COEFF = 5.0;

namespace {
    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovDegreees) {
        return (36.0 / 2) / std::tan((fovDegreees / 2) * (M_PI / 180));
    }

} // anonymous namespace


class QnActiParameterMap {
public:
    QnActiParameterMap() {};

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
#ifdef _DEBUG /* QMap is easier to look through in debug. */
    QMap<QString, QString> m_data;
#else
    QHash<QString, QString> m_data;
#endif
};


QnActiPtzController::QnActiPtzController(QnActiResource* resource):
    QnAbstractPtzController(resource),
    m_resource(resource),
    m_capabilities(Qn::NoCapabilities),
    m_spaceMapper(NULL),
    m_zoomInProgress(0),
    m_moveInProgress(0),
    m_minAngle(0),
    m_maxAngle(0)
{
    init();
}

QnActiPtzController::~QnActiPtzController() {
    return;
}

void QnActiPtzController::init() 
{
    CLHttpStatus status;
    QByteArray zoomString = m_resource->makeActiRequest(QLatin1String("encoder"), QLatin1String("ZOOM_CAP_GET"), status, true);
    if (status != CL_HTTP_SUCCESS || !zoomString.startsWith("ZOOM_CAP_GET="))
        return;

    m_capabilities |= Qn::AllPtzCapabilities;


    qreal minPanLogical = -17500, maxPanLogical = 17500; // todo: move to camera XML
    qreal minPanPhysical = 360, maxPanPhysical = 0; // todo: move to camera XML
    qreal minTiltLogical = 0, maxTiltLogical = 9000; //  // todo: move to camera XML
    qreal minTiltPhysical = -90, maxTiltPhysical = 0; //  // todo: move to camera XML
    qreal minAngle = 0, maxAngle = 0;

    QList<QByteArray> zoomParams = zoomString.split('=')[1].split(',');
    minAngle = m_resource->unquoteStr(zoomParams[0]).toInt();
    if (zoomParams.size() > 1)
        maxAngle = m_resource->unquoteStr(zoomParams[1]).toInt();


    /* These are in 1/10th of a degree, so we convert them away. */
    //minAngle /= 10.0;
    //maxAngle /= 10.0;

    /* We implement E-Flip, so we don't want strange tilt angles. */
    maxTiltPhysical = qMin(maxTiltPhysical, 90.0);
    minTiltPhysical = qMax(minTiltPhysical, -90.0);

    QnScalarSpaceMapper xMapper(minPanLogical, maxPanLogical, minPanPhysical, maxPanPhysical, Qn::PeriodicExtrapolation);
    QnScalarSpaceMapper yMapper(minTiltLogical, maxTiltLogical, minTiltPhysical, maxTiltPhysical, Qn::ConstantExtrapolation);

    m_minAngle = minAngle;
    m_maxAngle = maxAngle/DIGITAL_ZOOM_COEFF;

    QnScalarSpaceMapper zMapper(minAngle, maxAngle, m_minAngle, m_maxAngle, Qn::ConstantExtrapolation); // 30.972, 505.445
    //QnScalarSpaceMapper zMapper(minAngle, maxAngle, 1, 1000, Qn::ConstantExtrapolation); // 30.972, 505.445

    /* Note that we do not care about actual zoom limits on the camera. 
     * It's up to the camera to enforce them. */
    
    m_spaceMapper = new QnPtzSpaceMapper(QnVectorSpaceMapper(xMapper, yMapper, zMapper), QStringList());
}

int QnActiPtzController::stopZoomInternal()
{
    CLHttpStatus status;
    QByteArray result = m_resource->makeActiRequest(lit("encoder"), lit("ZOOM=STOP"), status);
    return status == CL_HTTP_SUCCESS ? 0 : -1;
}

int QnActiPtzController::stopMoveInternal()
{
    CLHttpStatus status;
    QByteArray result = m_resource->makeActiRequest(lit("encoder"), lit("MOVE=STOP"), status);
    return status == CL_HTTP_SUCCESS ? 0 : -1;
}

int sign(qreal value)
{
    return value >= 0 ? 1 : -1;
}

int scaleValue(qreal value, int min, int max)
{
    if (value == 0)
        return 0;

    return int (value * (max-min) + 0.5*sign(value)) + sign(value)*min;
}

int QnActiPtzController::startZoomInternal(qreal zoomVelocity)
{
    QString direction;
    if (zoomVelocity >= 0)
        direction = lit("TELE");
    else
        direction = lit("WIDE");

    int zoomVelocityI = qAbs(scaleValue(zoomVelocity, 2, 7));

    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(lit("encoder"), QString(lit("ZOOM=%1,%2")).arg(direction).arg(zoomVelocityI), status);
    int result = (status == CL_HTTP_SUCCESS ? 0 : -1);
    
    if (result == 0)
        m_zoomInProgress = true;

    return result;
}

int QnActiPtzController::startMoveInternal(qreal xVelocity, qreal yVelocity)
{
    QString direction;
    QString requestStr;

    if (xVelocity > 0)
    {
        if (yVelocity > 0)
            direction = lit("UPRIGHT");
        else if (yVelocity == 0)
            direction = lit("RIGHT");
        else 
            direction = lit("DOWNRIGHT");
    }
    else if (xVelocity == 0) 
    {
        if (yVelocity > 0)
            direction = lit("UP");
        else
            direction = lit("DOWN");
    }
    else {
        if (yVelocity > 0)
            direction = lit("UPLEFT");
        else if (yVelocity == 0)
            direction = lit("LEFT");
        else 
            direction = lit("DOWNLEFT");
    }


    int xVelocityI = qAbs(scaleValue(xVelocity, 1,5));
    int yVelocityI = qAbs(scaleValue(yVelocity, 1,5));

    if (xVelocity && yVelocity)
        requestStr = QString(lit("MOVE=%1,%2,%3")).arg(direction).arg(xVelocityI).arg(yVelocityI);
    else if (xVelocity)
        requestStr = QString(lit("MOVE=%1,%2")).arg(direction).arg(xVelocityI);
    else
        requestStr = QString(lit("MOVE=%1,%2")).arg(direction).arg(yVelocityI);

    CLHttpStatus status;
    QByteArray data = m_resource->makeActiRequest(lit("encoder"), requestStr, status);
    int result =  (status == CL_HTTP_SUCCESS ? 0 : -1);

    if (result == 0)
        m_moveInProgress = true;

    return result;
}

int QnActiPtzController::stopMove()
{
    QMutexLocker lock(&m_mutex);

    int result1 = 0, result2 = 0;
    if (m_zoomInProgress) {
        result1 = stopZoomInternal();
        m_zoomInProgress = false;
    }
    if (m_moveInProgress) {
        result2 = stopMoveInternal();
        m_moveInProgress = false;
    }

    if (result1)
        return result1;
    else if (result2)
        return result2;
    else
        return 0;
}

int QnActiPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) 
{
    QMutexLocker lock(&m_mutex);

    QByteArray result;
    int errCode = 0;

    stopMoveInternal();

    if (zoomVelocity) 
    {
        errCode = startZoomInternal(zoomVelocity);
        m_zoomInProgress = true;
    }
    else if (xVelocity || yVelocity) 
    {
        startMoveInternal(xVelocity, yVelocity);
        m_moveInProgress = true;
    }

    return errCode;
}

int QnActiPtzController::f35ZoomToNativeZoom(qreal f35Zoom)
{
    qreal percent = (f35Zoom - m_minAngle) / (m_maxAngle - m_minAngle);
    return percent * 1000 + 0.5;
}

qreal QnActiPtzController::nativeZoomToF35Zoom(int value)
{
    qreal percent = (value - m_minAngle) / (m_maxAngle*DIGITAL_ZOOM_COEFF - m_minAngle);
    return percent * (m_maxAngle-m_minAngle) + m_minAngle;
}

int QnActiPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) 
{
    QMutexLocker lock(&m_mutex);

    CLHttpStatus status;

    QByteArray result = m_resource->makeActiRequest(lit("encoder"), lit("POSITION=ABSOLUTE,%1,%2,5,5").arg(int(xPos)).arg(int(yPos)), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;

    result = m_resource->makeActiRequest(lit("encoder"), lit("ZOOM=DIRECT,%1").arg(f35ZoomToNativeZoom(zoomPos)), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;

    return 0;
}

int QnActiPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) 
{
    QMutexLocker lock(&m_mutex);

    CLHttpStatus status;

    QByteArray result = m_resource->makeActiRequest(lit("encoder"), lit("POSITION_GET"), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;
    QList<QByteArray> params = result.split(',');
    if (params.size() != 2)
        return -2;
    *xPos = params[0].toInt();
    *yPos = params[1].toInt();

    result = m_resource->makeActiRequest(lit("encoder"), lit("ZOOM_POSITION"), status);
    if (status != CL_HTTP_SUCCESS)
        return -1;
    
    *zoomPos = nativeZoomToF35Zoom(result.toInt());

    return 0;
}

Qn::CameraCapabilities QnActiPtzController::getCapabilities() {
    return m_capabilities;
}

const QnPtzSpaceMapper *QnActiPtzController::getSpaceMapper() {
    return m_spaceMapper;
}

