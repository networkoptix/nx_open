#include "caching_ptz_controller.h"

QnCachingPtzController::QnCachingPtzController(const QnPtzControllerPtr &baseController):
	base_type(baseController)
{
	
}

QnCachingPtzController::~QnCachingPtzController() {
	return;
}

bool QnCachingPtzController::extends(const QnPtzControllerPtr &baseController) {
	return 
        baseController->hasCapabilities(Qn::AsynchronousPtzCapability) &&
		!baseController->hasCapabilities(Qn::SynchronizedPtzCapability);
}

Qn::PtzCapabilities QnCachingPtzController::getCapabilities() {
	return baseController()->getCapabilities() | Qn::SynchronizedPtzCapability;
}

bool QnCachingPtzController::continuousMove(const QVector3D &speed) {
	
}

bool QnCachingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
	
}

bool QnCachingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
	
}

bool QnCachingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
	
}

bool QnCachingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
	
}

bool QnCachingPtzController::getFlip(Qt::Orientations *flip) {
	
}

bool QnCachingPtzController::createPreset(const QnPtzPreset &preset) {
	
}

bool QnCachingPtzController::updatePreset(const QnPtzPreset &preset) {
	
}

bool QnCachingPtzController::removePreset(const QString &presetId) {
	
}

bool QnCachingPtzController::activatePreset(const QString &presetId, qreal speed) {
	
}

bool QnCachingPtzController::getPresets(QnPtzPresetList *presets) {
	
}

bool QnCachingPtzController::createTour(const QnPtzTour &tour) {
	
}

bool QnCachingPtzController::removeTour(const QString &tourId) {
	
}

bool QnCachingPtzController::activateTour(const QString &tourId) {
	
}

bool QnCachingPtzController::getTours(QnPtzTourList *tours) {
	
}

bool QnCachingPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
	
}

bool QnCachingPtzController::synchronize(Qn::PtzDataFields query) {
	
}
