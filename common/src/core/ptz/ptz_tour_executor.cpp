#include "ptz_tour_executor.h"

#include "abstract_ptz_controller.h"


// -------------------------------------------------------------------------- //
// QnPtzTourExecutorPrivate
// -------------------------------------------------------------------------- //
class QnPtzTourExecutorPrivate {
public:
    enum State {
        Stopped,
        Waiting,
        Moving,
    };

    QnPtzTourExecutorPrivate(): currentState(Stopped) {}

    void startMoving() {
        assert(currentState == Stopped || currentState == Waiting);

        currentState = Moving;
        controller->getPosition(defaultSpace, &currentPosition);
        controller->activatePreset(currentTour.spots[currentIndex].presetId);
        //controller->syn

    }

    QnPtzControllerPtr controller;
    Qn::PtzCoordinateSpace defaultSpace;
    Qn::PtzDataField defaultDataField;
    
    State currentState;
    QnPtzTour currentTour;
    int currentIndex;
    QVector3D currentPosition;
};


// -------------------------------------------------------------------------- //
// QnPtzTourExecutor
// -------------------------------------------------------------------------- //
QnPtzTourExecutor::QnPtzTourExecutor(const QnPtzControllerPtr &controller):
    d(new QnPtzTourExecutorPrivate())
{
    d->controller = controller;
    d->defaultSpace = controller->hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalCoordinateSpace : Qn::DeviceCoordinateSpace;
    d->defaultDataField = d->defaultSpace == Qn::LogicalCoordinateSpace ? Qn::PtzLogicalPositionField : Qn::PtzDevicePositionField;
}

QnPtzTourExecutor::~QnPtzTourExecutor() {
    return;
}

void QnPtzTourExecutor::startTour(const QnPtzTour &tour) {
    stopTour();

    d->currentTour = tour;


    
}

void QnPtzTourExecutor::stopTour() {

}
