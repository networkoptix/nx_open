#ifdef ENABLE_ACTI

#include "acti_ptz_controller.h"

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include "acti_resource.h"

using namespace nx::core;
// -------------------------------------------------------------------------- //
// Utility
// -------------------------------------------------------------------------- //
namespace {
    struct ActiPtzVector {
        ActiPtzVector(): pan(0), tilt(0), zoom(0) {}
        ActiPtzVector(int pan, int tilt, int zoom): pan(pan), tilt(tilt), zoom(zoom) {}

        int pan, tilt, zoom;
    };

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ActiPtzVector, (eq), (pan)(tilt)(zoom));

    int toActiZoomSpeed(qreal zoomSpeed) {
#if 0
        zoomSpeed = qBound(-1.0, zoomSpeed, QnAbstractPtzController::MaxPtzSpeed);
        if(qFuzzyIsNull(zoomSpeed)) {
            return 0;
        } else {
            /* Zoom speed is an int in [2, 7] range. */
            return qMin(7, qFloor(2.0 + 6.0 * qAbs(zoomSpeed))) * qSign(zoomSpeed);
        }
#else
        /* Even though zoom speed is specified to be in [2, 7] range,
         * passing values other than 2 makes the camera ignore STOP commands.
         * This is the case for KCM8111, untested on other models => enabling it for all models. */
        if(qFuzzyIsNull(zoomSpeed)) {
            return 0;
        } else {
            return zoomSpeed > 0 ? 2 : -2;
        }
#endif
    }

    int toActiPanTiltSpeed(qreal panTiltSpeed) {
        panTiltSpeed = qBound(-QnAbstractPtzController::MaxPtzSpeed, panTiltSpeed, QnAbstractPtzController::MaxPtzSpeed);
        if(qFuzzyIsNull(panTiltSpeed)) {
            return 0;
        } else {
            return qMin(5, qFloor(1.0 + 5.0 * qAbs(panTiltSpeed))) * qSign(panTiltSpeed);
        }
    }

    ActiPtzVector toActiPtzSpeed(const nx::core::ptz::Vector& speedVector)
    {
        return ActiPtzVector(
            toActiPanTiltSpeed(speedVector.pan),
            toActiPanTiltSpeed(speedVector.tilt),
            toActiZoomSpeed(speedVector.zoom));
    }

    // TODO: #Elric use QnPtzUtilities
    QString actiZoomDirection(int deviceZoomSpeed) {
        if(deviceZoomSpeed < 0) {
            return lit("WIDE");
        } else if(deviceZoomSpeed == 0) {
            return lit("STOP");
        } else {
            return lit("TELE");
        }
    }

    QString actiPanTiltDirection(int devicePanSpeed, int deviceTiltSpeed) {
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

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnActiPtzControllerPrivate
// -------------------------------------------------------------------------- //
class QnActiPtzControllerPrivate {
public:
    enum {
        InvalidPtzValue = 0xDEADF00D
    };

    QnActiPtzControllerPrivate(const QnActiResourcePtr &resource):
        resource(resource),
        pendingCommand(Qn::InvalidPtzCommand),
        currentSpeed(InvalidPtzValue, InvalidPtzValue, InvalidPtzValue),
        currentPosition(InvalidPtzValue, InvalidPtzValue, InvalidPtzValue)
    {}

    virtual ~QnActiPtzControllerPrivate() {}

    void init();

    bool query(const QString& request, QByteArray* body = NULL, bool keepAllData = false) const;
    bool continuousZoomQuery(int zoomSpeed);
    bool continuousPanTiltQuery(int panSpeed, int tiltSpeed);
    bool absoluteZoomQuery(int zoom);
    bool absolutePanTiltQuery(int pan, int tilt, int panTiltSpeed);

    bool processQueries();
    bool processQueriesLocked();

    bool continuousMove(const ActiPtzVector& speed);
    bool absoluteMove(const ActiPtzVector& position, int panTiltSpeed);
    bool getPosition(ActiPtzVector* position);

public:
    QnMutex mutex;
    QnMutex queryMutex;

    /* Unprotected block. */

    QnActiResourcePtr resource;
    Ptz::Capabilities capabilities;
    Qt::Orientations flip;

    /* Block protected with mutex. */

    Qn::PtzCommand pendingCommand;
    ActiPtzVector pendingSpeed;
    ActiPtzVector pendingPosition;

    /* Block protected with queryMutex. */

    ActiPtzVector currentSpeed;
    ActiPtzVector currentPosition;
};

void QnActiPtzControllerPrivate::init() {
    QByteArray flipData;
    if(!query(lit("VIDEO_FLIP_MODE"), &flipData))
        return;
    if(flipData.toInt() == 1)
        flip |= Qt::Vertical;

    QByteArray mirrorData;
    if(!query(lit("VIDEO_MIRROR_MODE"), &mirrorData))
        return;
    if(mirrorData.toInt() == 1)
        flip |= Qt::Horizontal;

    QByteArray zoomData;
    if(!query(lit("ZOOM_CAP_GET"), &zoomData, true))
        return;
    if(!zoomData.startsWith("ZOOM_CAP_GET="))
        return;

    capabilities = Ptz::ContinuousPtzCapabilities | Ptz::AbsolutePtzCapabilities | Ptz::DevicePositioningPtzCapability | Ptz::FlipPtzCapability;

    // TODO: #PTZ
#if 0
    if (!m_isFlipped)
        m_capabilities &= ~Ptz::AbsolutePtzCapabilities; // acti 8111 has bug for absolute position if flip turned off
#endif
}

bool QnActiPtzControllerPrivate::query(
    const QString& request, QByteArray* body, bool keepAllData) const
{
    CLHttpStatus status;
    QByteArray data = resource->makeActiRequest(lit("encoder"), request, status, keepAllData);
    if(body) {
        if (data.startsWith("OK: "))
            data = data.mid(4);
        *body = data;
    }
    return status == CL_HTTP_SUCCESS;
}

bool QnActiPtzControllerPrivate::continuousZoomQuery(int zoomSpeed)
{
    QString request = lit("ZOOM=%1").arg(actiZoomDirection(zoomSpeed));
    if(zoomSpeed != 0)
        request += lit(",%1").arg(qAbs(zoomSpeed));

    return query(request);
}

bool QnActiPtzControllerPrivate::continuousPanTiltQuery(int panSpeed, int tiltSpeed)
{
    QString request = lit("MOVE=%1").arg(actiPanTiltDirection(panSpeed, tiltSpeed));
    if (panSpeed != 0)
        request += lit(",%1").arg(qAbs(panSpeed));
    if (tiltSpeed != 0)
        request += lit(",%1").arg(qAbs(tiltSpeed));

    return query(request);
}

bool QnActiPtzControllerPrivate::absoluteZoomQuery(int zoom)
{
    return query(lit("ZOOM=DIRECT,%1").arg(zoom));
}

bool QnActiPtzControllerPrivate::absolutePanTiltQuery(int pan, int tilt, int panTiltSpeed)
{
    return query(lit("POSITION=ABSOLUTE,%1,%2,%3,%3").arg(pan).arg(tilt).arg(panTiltSpeed));
}

bool QnActiPtzControllerPrivate::processQueriesLocked()
{
    bool result = true;
    bool first = true;

    while(true)
    {
        Qn::PtzCommand command;
        ActiPtzVector speed;
        ActiPtzVector position;
        bool status = true;

        {
            QnMutexLocker locker( &mutex );
            command = pendingCommand;
            speed = pendingSpeed;
            position = pendingPosition;

            pendingCommand = Qn::InvalidPtzCommand;
        }

        switch(command)
        {
        case Qn::ContinuousMovePtzCommand:
        {
            if(currentSpeed == speed)
                break;

            /* Stop first. */
            if(currentSpeed.zoom != 0)
                status = status & continuousZoomQuery(0);
            if(currentSpeed.pan != 0 || currentSpeed.tilt != 0)
                status = status & continuousPanTiltQuery(0, 0);

            /* Then move. */
            if(speed.zoom != 0)
                status = status & continuousZoomQuery(speed.zoom);
            if(speed.pan != 0 || speed.tilt != 0)
                status = status & continuousPanTiltQuery(speed.pan, speed.tilt);

            if(status) {
                currentSpeed = speed;
                if(speed.zoom != 0)
                    currentPosition.zoom = InvalidPtzValue;
                if(speed.pan != 0 || speed.tilt != 0)
                    currentPosition.pan = currentPosition.tilt = InvalidPtzValue;
            } else {
                currentSpeed = currentPosition = ActiPtzVector(InvalidPtzValue, InvalidPtzValue, InvalidPtzValue);
            }

            break;
        }
        case Qn::AbsoluteDeviceMovePtzCommand:
        {
            /* Stop first.
             * If we don't do that, absolute movement will not work. */
            if(currentSpeed.zoom != 0)
                status = status & continuousZoomQuery(0);
            if(currentSpeed.pan != 0 || currentSpeed.tilt != 0)
                status = status & continuousPanTiltQuery(0, 0);

            /* Issue commands EVEN if position wasn't changed.
             * This is because acti cameras sometimes outright ignore absolute move commands. */
            status = status & absolutePanTiltQuery(position.pan, position.tilt, speed.pan);
            status = status & absoluteZoomQuery(position.zoom);

            if(status)
            {
                currentPosition = position;
                currentSpeed = ActiPtzVector(0, 0, 0);
            } else
            {
                currentSpeed = currentPosition = ActiPtzVector(InvalidPtzValue, InvalidPtzValue, InvalidPtzValue);
            }

            break;
        }
        default:
            return result;
        }

        if (first)
        {
            first = false;
            result = status;
        }
    }
}

bool QnActiPtzControllerPrivate::processQueries()
{
    if (!queryMutex.tryLock())
        return true; /* Just assume that it will work. */

    bool result = processQueriesLocked();

    queryMutex.unlock();

    return result;
}

bool QnActiPtzControllerPrivate::continuousMove(const ActiPtzVector& speed)
{
    {
        QnMutexLocker locker( &mutex );
        pendingSpeed = speed;
        pendingCommand = Qn::ContinuousMovePtzCommand;
    }

    processQueries();
    return true;
}

bool QnActiPtzControllerPrivate::absoluteMove(const ActiPtzVector& position, int panTiltSpeed)
{
    {
        QnMutexLocker locker( &mutex );
        pendingPosition = position;
        pendingSpeed = ActiPtzVector(panTiltSpeed, panTiltSpeed, 0);
        pendingCommand = Qn::AbsoluteDeviceMovePtzCommand;
    }

    processQueries();
    return true;
}

bool QnActiPtzControllerPrivate::getPosition(ActiPtzVector* position)
{
    /* Always send the requests, don't use cached values. This is safer. */

    QByteArray positionData;
    if(!query(lit("POSITION_GET"), &positionData))
        return false;

    QByteArray zoomData;
    if(!query(lit("ZOOM_POSITION"), &zoomData))
        return false;

    QList<QByteArray> panTiltData = positionData.split(',');
    if (panTiltData.size() != 2)
        return false;

    position->pan = panTiltData[0].toInt();
    position->tilt = panTiltData[1].toInt();
    position->zoom = zoomData.toInt();
    return true;
}

// -------------------------------------------------------------------------- //
// QnActiPtzController
// -------------------------------------------------------------------------- //
QnActiPtzController::QnActiPtzController(const QnActiResourcePtr& resource):
    base_type(resource),
    d(new QnActiPtzControllerPrivate(resource))
{
    d->init();
}

QnActiPtzController::~QnActiPtzController()
{
    return;
}

Ptz::Capabilities QnActiPtzController::getCapabilities(const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
        return Ptz::NoPtzCapabilities;

    return d->capabilities;
}

bool QnActiPtzController::continuousMove(
    const nx::core::ptz::Vector& speedVector,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Continuous movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(d->resource->getName(), d->resource->getId()));

        return false;
    }

    return d->continuousMove(toActiPtzSpeed(speedVector));
}

bool QnActiPtzController::absoluteMove(
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options)
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Absolute movement - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(d->resource->getName(), d->resource->getId()));

        return false;
    }

    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    // We don't want to get zero speed, hence 0.01 bound.
    return d->absoluteMove(
        ActiPtzVector(
            position.pan,
            position.tilt,
            position.zoom),
        toActiPanTiltSpeed(qBound(0.01, speed, 1.0)));
}

bool QnActiPtzController::getPosition(
    Qn::PtzCoordinateSpace space,
    nx::core::ptz::Vector* outPosition,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting current position - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(d->resource->getName(), d->resource->getId()));

        return false;
    }

    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    ActiPtzVector devicePosition;
    if(!d->getPosition(&devicePosition))
        return false;

    *outPosition = nx::core::ptz::Vector(
        devicePosition.pan,
        devicePosition.tilt,
        /*rotation*/ 0.0,
        devicePosition.zoom);
    return true;
}

bool QnActiPtzController::getFlip(
    Qt::Orientations *flip,
    const nx::core::ptz::Options& options) const
{
    if (options.type != ptz::Type::operational)
    {
        NX_WARNING(
            this,
            lm("Getting flip - wrong PTZ type. "
                "Only operational PTZ is supported. Resource %1 (%2)")
                .args(d->resource->getName(), d->resource->getId()));

        return false;
    }

    *flip = d->flip;
    return true;
}

#endif // ENABLE_ACTI
