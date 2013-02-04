#include "dw_zoom_ptz_controller.h"

#include <plugins/resources/camera_settings/camera_settings.h>

#include "digital_watchdog_resource.h"

QnDwZoomPtzController::QnDwZoomPtzController(const QnPlWatchDogResourcePtr &resource):
    QnAbstractPtzController(resource),
    m_resource(resource)
{}

QnDwZoomPtzController::~QnDwZoomPtzController() {
    return;
}

int QnDwZoomPtzController::startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) {
    CameraSetting setting(
        QLatin1String("%%Lens%%Zoom"),
        QLatin1String("Zoom"),
        CameraSetting::ControlButtonsPair,
        QString(),
        QString(),
        QString(),
        CameraSettingValue(QLatin1String("zoomOut")),
        CameraSettingValue(QLatin1String("zoomIn")),
        CameraSettingValue(QLatin1String("stop")),
        QString()
    );

    if(qFuzzyIsNull(zoomVelocity)) {
        setting.setCurrent(setting.getStep());
    } else if(zoomVelocity < 0.0) {
        setting.setCurrent(setting.getMin());
    } else if(zoomVelocity > 0.0) {
        setting.setCurrent(setting.getMax());
    }

    m_resource->setParam(setting.getId(), setting.serializeToStr(), QnDomainPhysical);

    return 0;
}

int QnDwZoomPtzController::stopMove() {
    return startMove(0.0, 0.0, 0.0);
}

int QnDwZoomPtzController::moveTo(qreal xPos, qreal yPos, qreal zoomPos) {
    return 1;
}

int QnDwZoomPtzController::getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) {
    return 1;
}

Qn::CameraCapabilities QnDwZoomPtzController::getCapabilities() {
    return Qn::ContinuousZoomCapability;
}

const QnPtzSpaceMapper *QnDwZoomPtzController::getSpaceMapper() {
    return NULL;
}
