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

int QnDwZoomPtzController::startMove(const QVector3D &speed) {
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

    if(qFuzzyIsNull(speed.z())) {
        setting.setCurrent(setting.getStep());
    } else if(speed.z() < 0.0) {
        setting.setCurrent(setting.getMin());
    } else if(speed.z() > 0.0) {
        setting.setCurrent(setting.getMax());
    }

    m_resource->setParam(setting.getId(), setting.serializeToStr(), QnDomainPhysical);

    return 0;
}

int QnDwZoomPtzController::stopMove() {
    return startMove(QVector3D(0.0, 0.0, 0.0));
}

int QnDwZoomPtzController::setPosition(const QVector3D &) {
    return 1;
}

int QnDwZoomPtzController::getPosition(QVector3D *) {
    return 1;
}

Qn::PtzCapabilities QnDwZoomPtzController::getCapabilities() {
    return Qn::ContinuousZoomCapability;
}

