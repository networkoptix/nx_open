#include "camera_settings_model.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnCameraSettingsModel::QnCameraSettingsModel(QObject *parent):
    base_type(parent),
    m_updating(false)
{

}

QnCameraSettingsModel::~QnCameraSettingsModel() {

}

QnVirtualCameraResourcePtr QnCameraSettingsModel::camera() const {
    return m_camera;
}

void QnCameraSettingsModel::setCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_camera == camera)
        return;

    if (m_camera)
        disconnect(m_camera, NULL, this, NULL);

    m_camera = camera;

    if (m_camera) {
        connect(m_camera, &QnResource::urlChanged,  this, &QnCameraSettingsModel::at_camera_urlChanged);
        at_camera_urlChanged(m_camera);
    }


}

void QnCameraSettingsModel::at_camera_urlChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera || camera != m_camera)
        return;

    QString urlString = camera->getUrl();
    QUrl url = QUrl::fromUserInput(urlString);
    
    setAddress(!url.isEmpty() && url.isValid() ? url.host() : urlString);
    
    QString webPageAddress = QString(QLatin1String("http://%1")).arg(camera->getHostAddress());
    if(url.isValid()) {
        QUrlQuery query(url);
        int port = query.queryItemValue(lit("http_port")).toInt();
        if(port == 0)
            port = url.port(80);

        if (port != 80 && port > 0)
            webPageAddress += L':' + QString::number(url.port());
    }

    QString webPage = lit("<a href=\"%1\">%2</a>").arg(webPageAddress).arg(webPageAddress);
    setWebPage(webPage);
}

QString QnCameraSettingsModel::address() const {
    return m_address;
}

void QnCameraSettingsModel::setAddress(const QString &address) {
    if (m_address == address)
        return;
    m_address = address;
    if (!m_updating)
        emit addressChanged(address);
}

QString QnCameraSettingsModel::webPage() const {
    return m_webPage;
}

void QnCameraSettingsModel::setWebPage(const QString &webPage) {
    if (m_webPage == webPage)
        return;
    m_webPage = webPage;
    if (!m_updating)
        emit webPageChanged(webPage);
}

