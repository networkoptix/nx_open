#include "tour_ptz_controller.h"

#include <utils/common/json.h>
#include <utils/common/container.h>

#include <api/kvpair_usage_helper.h>


// -------------------------------------------------------------------------- //
// QnTourPtzControllerPrivate
// -------------------------------------------------------------------------- //
class QnTourPtzControllerPrivate {
public:
    QnTourPtzControllerPrivate(): helper(NULL) {}

    void loadRecords() {
        if(!records.isEmpty())
            return;

        if(helper->value().isEmpty()) {
            records.clear();
        } else {
            QJson::deserialize(helper->value().toUtf8(), &records);
        }
    }

    void saveRecords() {
        helper->setValue(QString::fromUtf8(QJson::serialized(records)));
    }

    QMutex mutex;
    QnStringKvPairUsageHelper *helper;
    QHash<QString, QnPtzTour> records;
};


// -------------------------------------------------------------------------- //
// QnTourPtzController
// -------------------------------------------------------------------------- //
QnTourPtzController::QnTourPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    d(new QnTourPtzControllerPrivate())
{
    d->helper = new QnStringKvPairUsageHelper(resource(), lit("ptzTours"), QString(), this);
}

QnTourPtzController::~QnTourPtzController() {
    return;
}

bool QnTourPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::PresetsPtzCapability) &&
        !baseController->hasCapabilities(Qn::ToursPtzCapability);
}

Qn::PtzCapabilities QnTourPtzController::getCapabilities() {
    /* Note that this controller preserves the Qn::NonBlockingPtzCapability. */
    return baseController()->getCapabilities() | Qn::ToursPtzCapability;
}

bool QnTourPtzController::createTour(const QnPtzTour &tour) {
    return createTourInternal(tour);
}

bool QnTourPtzController::createTourInternal(QnPtzTour tour) {
    if(tour.id.isEmpty())
        return false;

    /* We need to check validity of the tour first. */
    QnPtzPresetList presets;
    if(!getPresets(&presets))
        return false;

    for(int i = 0; i < tour.spots.size(); i++) {
        QnPtzTourSpot &spot = tour.spots[i];

        int index = qnIndexOf(presets, [&](const QnPtzPreset &preset) { return spot.presetId == preset.id; });
        if(index == -1)
            return false;

        /* Also fix invalid parameters. */
        spot.stayTime = qMax(spot.stayTime, 0ll);
        spot.speed = qBound<qreal>(0.01, spot.speed, 1.0);
    }

    /* Tour is fine, save it. */
    QMutexLocker locker(&d->mutex);
    d->loadRecords();
    d->records.insert(tour.id, tour);
    d->saveRecords();
    return true;
}

bool QnTourPtzController::removeTour(const QString &tourId) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();
    if(d->records.remove(tourId) == 0)
        return false;
    d->saveRecords();
    return true;
}

bool QnTourPtzController::activateTour(const QString &tourId) {
    QnPtzTour tour;
    {
        QMutexLocker locker(&d->mutex);
        d->loadRecords();
        if(!d->records.contains(tourId))
            return false;
        tour = d->records[tourId];
    }

    /*if(!absoluteMove(data.space, data.position))    
        return false;*/

    return true;
}

bool QnTourPtzController::getTours(QnPtzTourList *tours) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();
    *tours = d->records.values();
    return true;
}
