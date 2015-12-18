/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#include "camera_advanced_settings_web_page.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkCookie>
#include "core/resource/resource.h"
#include "http/custom_headers.h"

namespace {
    class CustomCookieJar : public QNetworkCookieJar
    {
    public:
        CustomCookieJar(QObject* parent = 0) : QNetworkCookieJar(parent) {}

        void setCamera(QnResourcePtr camRes)
        {
            m_camRes = camRes;
        }

        virtual QList<QNetworkCookie> cookiesForUrl(const QUrl & url) const override
        {
            QN_UNUSED(url);
            QList<QNetworkCookie> result;
            if (m_camRes)
                result << QNetworkCookie(Qn::CAMERA_GUID_HEADER_NAME, m_camRes->getId().toByteArray());
            return result;
        }
    private:
        QnResourcePtr m_camRes;
    };
}

CameraAdvancedSettingsWebPage::CameraAdvancedSettingsWebPage(QWebEngineProfile *profile, QObject* parent )
    : QWebEnginePage(profile, parent )
    , m_cookieJar(new CustomCookieJar(this))
{
    //TODO: #GDM fix 5.6 webengine
    // networkAccessManager()->setCookieJar(m_cookieJar);
}

void CameraAdvancedSettingsWebPage::setCamera(const QnResourcePtr & camera)
{
    if (CustomCookieJar* jar = dynamic_cast<CustomCookieJar *>(m_cookieJar))
        jar->setCamera(camera);
}
