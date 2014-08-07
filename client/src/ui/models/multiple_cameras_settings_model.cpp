#include "multiple_cameras_settings_model.h"

#include <ui/models/camera_settings_model.h>

QnMultipleCamerasSettingsModel::QnMultipleCamerasSettingsModel(QObject *parent /*= NULL*/):
    base_type(parent)
{

}

QnMultipleCamerasSettingsModel::~QnMultipleCamerasSettingsModel() {
}

QnVirtualCameraResourceList QnMultipleCamerasSettingsModel::cameras() const {
    QnVirtualCameraResourceList result;
    foreach (QnCameraSettingsModel* model, m_models)
        result << model->camera();
}

void QnMultipleCamerasSettingsModel::setCameras(const QnVirtualCameraResourceList &cameras) {
    while (!m_models.isEmpty()) {
        delete m_models.takeFirst();
    }
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        auto model = new QnCameraSettingsModel(this);
        model->setCamera(camera);
        //connect to model
        m_models << model;
    }

}

void QnMultipleCamerasSettingsModel::updateFromResources() {
    foreach (QnCameraSettingsModel* model, m_models)
        model->updateFromResource();
}

void QnMultipleCamerasSettingsModel::submitToResources() {

}
