#ifdef ENABLE_ACTI

#include "acti_ptz_controller.h"

#include <utils/common/model_functions.h>

#include "acti_resource.h"

namespace {

    int qSign(qreal value) {
        return value >= 0 ? 1 : -1;
    }

    // ACTi 8111
    /*qreal pointsData[][2] = {
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
    };*/

} // anonymous namespace


// TODO: #Elric #PTZ use mutex

// -------------------------------------------------------------------------- //
// Utility
// -------------------------------------------------------------------------- //
namespace {
    struct ActiPtzVector {
        ActiPtzVector(): pan(0), tilt(0), zoom(0) {}
        ActiPtzVector(int pan, int tilt, int zoom): pan(pan), tilt(tilt), zoom(zoom) {}

        QN_DEFINE_STRUCT_OPERATOR_EQ(ActiPtzVector, (pan)(tilt)(zoom), friend);

        int pan, tilt, zoom;
    };

    int toActiZoomSpeed(qreal zoomSpeed) {
#if 0
        zoomSpeed = qBound(-1.0, zoomSpeed, 1.0);
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
        panTiltSpeed = qBound(-1.0, panTiltSpeed, 1.0);
        if(qFuzzyIsNull(panTiltSpeed)) {
            return 0;
        } else {
            return qMin(5, qFloor(1.0 + 5.0 * qAbs(panTiltSpeed))) * qSign(panTiltSpeed);
        }
    }

    ActiPtzVector toActiPtzSpeed(const QVector3D &speed) {
        return ActiPtzVector(
            toActiPanTiltSpeed(speed.x()),
            toActiPanTiltSpeed(speed.y()),
            toActiZoomSpeed(speed.z())
        );
    }

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

    QnActiPtzControllerPrivate(const QnActiResourcePtr &resource): resource(resource), pendingCommand(Qn::InvalidPtzCommand) {}
    virtual ~QnActiPtzControllerPrivate() {}

    void init();
    
    bool query(const QString &request, QByteArray *body = NULL, bool keepAllData = false) const;
    bool continuousZoomQuery(int zoomSpeed);
    bool continuousPanTiltQuery(int panSpeed, int tiltSpeed);
    bool absoluteZoomQuery(int zoom);
    bool absolutePanTiltQuery(int pan, int tilt, int panTiltSpeed);

    bool processQueries();
    bool processQueriesLocked();

    bool continuousMove(const ActiPtzVector &speed);
    bool absoluteMove(const ActiPtzVector &position, int panTiltSpeed);
    bool getPosition(ActiPtzVector *position);

public:
    QMutex mutex;
    QMutex queryMutex;

    QnActiResourcePtr resource;
    Qn::PtzCapabilities capabilities;
    Qt::Orientations flip;

    Qn::PtzCommand pendingCommand;
    ActiPtzVector pendingSpeed;
    ActiPtzVector pendingPosition;

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

    capabilities = Qn::NoPtzCapabilities;
    if(resource->getModel() == lit("KCM3311")) {
        capabilities |= Qn::ContinuousZoomCapability;
    } else {
        capabilities |= Qn::ContinuousPtzCapabilities | Qn::AbsolutePtzCapabilities | Qn::DevicePositioningPtzCapability;
    }
    capabilities |= Qn::FlipPtzCapability;

#if 0
    if (!m_isFlipped)
        m_capabilities &= ~Qn::AbsolutePtzCapabilities; // acti 8111 has bug for absolute position if flip turned off
#endif
}

bool QnActiPtzControllerPrivate::query(const QString &request, QByteArray *body, bool keepAllData) const {
    qDebug() << "ACTI REQUEST" << request;

    CLHttpStatus status;
    QByteArray data = resource->makeActiRequest(lit("encoder"), request, status, keepAllData);
    if(body)
        *body = data;
    return status == CL_HTTP_SUCCESS;
}

bool QnActiPtzControllerPrivate::continuousZoomQuery(int zoomSpeed) {
    QString request = lit("ZOOM=%1").arg(actiZoomDirection(zoomSpeed));
    if(zoomSpeed != 0)
        request += lit(",%1").arg(qAbs(zoomSpeed));

    return query(request);
}

bool QnActiPtzControllerPrivate::continuousPanTiltQuery(int panSpeed, int tiltSpeed) {
    QString request = lit("MOVE=%1").arg(actiPanTiltDirection(panSpeed, tiltSpeed));
    if (panSpeed != 0)
        request += lit(",%1").arg(qAbs(panSpeed));
    if (tiltSpeed != 0)
        request += lit(",%1").arg(qAbs(tiltSpeed));

    return query(request);
}

bool QnActiPtzControllerPrivate::absoluteZoomQuery(int zoom) {
    return query(lit("ZOOM=DIRECT,%1").arg(zoom));
}

bool QnActiPtzControllerPrivate::absolutePanTiltQuery(int pan, int tilt, int panTiltSpeed) {
    return query(lit("POSITION=ABSOLUTE,%1,%2,%3,%3").arg(pan).arg(tilt).arg(panTiltSpeed));
}

bool QnActiPtzControllerPrivate::processQueriesLocked() {
    bool result = true;
    bool first = true;

    while(true) {
        Qn::PtzCommand command;
        ActiPtzVector speed;
        ActiPtzVector position;
        bool status = true;

        {
            QMutexLocker locker(&mutex);
            command = pendingCommand;
            speed = pendingSpeed;
            position = pendingPosition;

            pendingCommand = Qn::InvalidPtzCommand;
        }

        switch(command) {
        case Qn::ContinuousMovePtzCommand: {
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
        case Qn::AbsoluteDeviceMovePtzCommand: {
            if(currentPosition == position)
                break;

            if(currentPosition.pan != position.pan || currentPosition.tilt != position.tilt)
                status = status & absolutePanTiltQuery(position.pan, position.tilt, speed.pan);

            if(currentPosition.zoom != position.zoom)
                status = status & absoluteZoomQuery(position.zoom);

            if(status) {
                currentPosition = position;
                currentSpeed = ActiPtzVector(0, 0, 0);
            } else {
                currentSpeed = currentPosition = ActiPtzVector(InvalidPtzValue, InvalidPtzValue, InvalidPtzValue);
            }

            break;
        }
        default:
            return result;
        }

        if(first) {
            first = false;
            result = status;
        }
    }
}

bool QnActiPtzControllerPrivate::processQueries() {
    if(!queryMutex.tryLock())
        return true; /* Just assume that it will work. */

    bool result = processQueriesLocked();

    queryMutex.unlock();

    return result;
}

bool QnActiPtzControllerPrivate::continuousMove(const ActiPtzVector &speed) {
    {
        QMutexLocker locker(&mutex);
        pendingSpeed = speed;
        pendingCommand = Qn::ContinuousMovePtzCommand;
    }

    processQueries();
    return true;
}

bool QnActiPtzControllerPrivate::absoluteMove(const ActiPtzVector &position, int panTiltSpeed) {
    {
        QMutexLocker locker(&mutex);
        pendingPosition = position;
        pendingSpeed = ActiPtzVector(panTiltSpeed, panTiltSpeed, 0);
        pendingCommand = Qn::AbsoluteDeviceMovePtzCommand;
    }

    processQueries();
    return true;
}

bool QnActiPtzControllerPrivate::getPosition(ActiPtzVector *position) {
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
QnActiPtzController::QnActiPtzController(const QnActiResourcePtr &resource):
    base_type(resource),
    d(new QnActiPtzControllerPrivate(resource))
{
    d->init();
}

QnActiPtzController::~QnActiPtzController() {
    return;
}

Qn::PtzCapabilities QnActiPtzController::getCapabilities() {
    return d->capabilities;
}

bool QnActiPtzController::continuousMove(const QVector3D &speed) {
    return d->continuousMove(toActiPtzSpeed(speed));
}

bool QnActiPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;
    
    return d->absoluteMove(ActiPtzVector(position.x(), position.y(), position.z()), toActiPanTiltSpeed(qBound(0.01, speed, 1.0))); /* We don't want to get zero speed, hence 0.01 bound. */
}

bool QnActiPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(space != Qn::DevicePtzCoordinateSpace)
        return false;

    ActiPtzVector devicePosition;
    if(!d->getPosition(&devicePosition))
        return false;

    *position = QVector3D(devicePosition.pan, devicePosition.tilt, devicePosition.zoom);
    return true;
}

bool QnActiPtzController::getFlip(Qt::Orientations *flip) {
    *flip = d->flip;
    return true;
}

#endif // ENABLE_ACTI
