#include "context_settings.h"

#include "mobile_client/mobile_client_settings.h"

QnContextSettings::QnContextSettings(QObject *parent) :
    QObject(parent)
{
    connect(qnSettings, &QnMobileClientSettings::valueChanged, this, &QnContextSettings::at_valueChanged);
}

bool QnContextSettings::showOfflineCameras() const {
    return qnSettings->showOfflineCameras();
}

void QnContextSettings::setShowOfflineCameras(bool showOfflineCameras) {
    qnSettings->setShowOfflineCameras(showOfflineCameras);
}

void QnContextSettings::at_valueChanged(int valueId) {
    switch (valueId) {
    case QnMobileClientSettings::ShowOfflineCameras:
        emit showOfflineCamerasChanged();
        break;
    default:
        break;
    }
}
