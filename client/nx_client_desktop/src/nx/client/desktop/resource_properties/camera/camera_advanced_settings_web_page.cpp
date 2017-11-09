#include "camera_advanced_settings_web_page.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkCookie>

#include <core/resource/camera_resource.h>

#include <nx/network/http/custom_headers.h>

namespace nx {
namespace client {
namespace desktop {

class CameraAdvancedSettingsWebPage::CustomCookieJar: public QNetworkCookieJar
{
public:
    CustomCookieJar(QObject* parent = 0): QNetworkCookieJar(parent) {}

    QnVirtualCameraResourcePtr camera() const
    {
        return m_camera;
    }

    void setCamera(const QnVirtualCameraResourcePtr& camera)
    {
        m_camera = camera;
    }

    virtual QList<QNetworkCookie> cookiesForUrl(const QUrl& /*url*/) const override
    {
        QList<QNetworkCookie> result;
        if (m_camera)
            result << QNetworkCookie(Qn::CAMERA_GUID_HEADER_NAME, m_camera->getId().toByteArray());
        return result;
    }
private:
    QnVirtualCameraResourcePtr m_camera;
};

CameraAdvancedSettingsWebPage::CameraAdvancedSettingsWebPage(QObject* parent):
    base_type(parent)
{
    m_cookieJar = new CustomCookieJar(this);
    networkAccessManager()->setCookieJar(m_cookieJar);
}

QnVirtualCameraResourcePtr CameraAdvancedSettingsWebPage::camera() const
{
    return  m_cookieJar->camera();
}

QString	CameraAdvancedSettingsWebPage::userAgentForUrl(const QUrl& /*url*/) const
{
    //this User-Agent is required for vista camera to use html/js page, not java applet
    return lit("Mozilla/5.0 (Windows; U; Windows NT based; en-US) AppleWebKit/534.34 (KHTML, like Gecko)  QtWeb Internet Browser/3.8.5 http://www.QtWeb.net");
}

void CameraAdvancedSettingsWebPage::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_cookieJar->setCamera(camera);
}

} // namespace desktop
} // namespace client
} // namespace nx
