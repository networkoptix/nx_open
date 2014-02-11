#include "notifying_ptz_controller.h"

#include <common/common_meta_types.h>

QnNotifyingPtzController::QnNotifyingPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController)
{}

QnNotifyingPtzController::~QnNotifyingPtzController() {
    return;
}

bool QnNotifyingPtzController::extends(Qn::PtzCapabilities capabilities) {
    return !(capabilities & Qn::AsynchronousPtzCapability);
}

Qn::PtzCapabilities QnNotifyingPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::AsynchronousPtzCapability;
}

bool QnNotifyingPtzController::continuousMove(const QVector3D &speed) {
    bool result = base_type::continuousMove(speed);
    emit finishedLater(Qn::ContinuousMovePtzCommand, result ? QVariant::fromValue(speed) : QVariant());
    return result;
}

bool QnNotifyingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    bool result = base_type::absoluteMove(space, position, speed);
    emit finishedLater(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space), result ? QVariant::fromValue(position) : QVariant());
    return result;
}

bool QnNotifyingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    bool result = base_type::viewportMove(aspectRatio, viewport, speed);
    emit finishedLater(Qn::ViewportMovePtzCommand, result ? QVariant::fromValue(viewport) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    bool result = base_type::getPosition(space, position);
    emit finishedLater(spaceCommand(Qn::GetDevicePositionPtzCommand, space), result ? QVariant::fromValue(*position) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    bool result = base_type::getLimits(space, limits);
    emit finishedLater(spaceCommand(Qn::GetDeviceLimitsPtzCommand, space), result ? QVariant::fromValue(*limits) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getFlip(Qt::Orientations *flip) {
    bool result = base_type::getFlip(flip);
    emit finishedLater(Qn::GetFlipPtzCommand, result ? QVariant::fromValue(*flip) : QVariant());
    return result;
}

bool QnNotifyingPtzController::createPreset(const QnPtzPreset &preset) {
    bool result = base_type::createPreset(preset);
    emit finishedLater(Qn::CreatePresetPtzCommand, result ? QVariant::fromValue(preset) : QVariant());
    return result;
}

bool QnNotifyingPtzController::updatePreset(const QnPtzPreset &preset) {
    bool result = base_type::updatePreset(preset);
    emit finishedLater(Qn::UpdatePresetPtzCommand, result ? QVariant::fromValue(preset) : QVariant());
    return result;
}

bool QnNotifyingPtzController::removePreset(const QString &presetId) {
    bool result = base_type::removePreset(presetId);
    emit finishedLater(Qn::RemovePresetPtzCommand, result ? QVariant::fromValue(presetId) : QVariant());
    return result;
}

bool QnNotifyingPtzController::activatePreset(const QString &presetId, qreal speed) {
    bool result = base_type::activatePreset(presetId, speed);
    emit finishedLater(Qn::ActivatePresetPtzCommand, result ? QVariant::fromValue(presetId) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getPresets(QnPtzPresetList *presets) {
    bool result = base_type::getPresets(presets);
    emit finishedLater(Qn::GetPresetsPtzCommand, result ? QVariant::fromValue(*presets) : QVariant());
    return result;
}

bool QnNotifyingPtzController::createTour(const QnPtzTour &tour) {
    bool result = base_type::createTour(tour);
    emit finishedLater(Qn::CreateTourPtzCommand, result ? QVariant::fromValue(tour) : QVariant());
    return result;
}

bool QnNotifyingPtzController::removeTour(const QString &tourId) {
    bool result = base_type::removeTour(tourId);
    emit finishedLater(Qn::RemoveTourPtzCommand, result ? QVariant::fromValue(tourId) : QVariant());
    return result;
}

bool QnNotifyingPtzController::activateTour(const QString &tourId) {
    bool result = base_type::activateTour(tourId);
    emit finishedLater(Qn::ActivateTourPtzCommand, result ? QVariant::fromValue(tourId) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getTours(QnPtzTourList *tours) {
    bool result = base_type::getTours(tours);
    emit finishedLater(Qn::GetToursPtzCommand, result ? QVariant::fromValue(*tours) : QVariant());
    return result;
}

bool QnNotifyingPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    bool result = base_type::getData(query, data);
    emit finishedLater(Qn::GetDataPtzCommand, result ? QVariant::fromValue(*data) : QVariant());
    return result;
}

