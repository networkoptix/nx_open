#ifdef ENABLE_ONVIF

#include "dw_zoom_ptz_controller.h"

#include <plugins/resource/camera_settings/camera_settings.h>

#include "digital_watchdog_resource.h"

QnDwZoomPtzController::QnDwZoomPtzController(const QnPlWatchDogResourcePtr &resource):
    base_type(resource),
    m_resource(resource)
{}

QnDwZoomPtzController::~QnDwZoomPtzController() {
    return;
}

Qn::PtzCapabilities QnDwZoomPtzController::getCapabilities() {
    return Qn::ContinuousZoomCapability;
}

bool QnDwZoomPtzController::continuousMove(const QVector3D &speed) {
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

    m_resource->setParamPhysical(setting.getId(), setting.serializeToStr());

    return true;
}

#endif //ENABLE_ONVIF
