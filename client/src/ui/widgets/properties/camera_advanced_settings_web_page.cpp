/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#include "camera_advanced_settings_web_page.h"
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include "core/resource/resource.h"

class QnCustomCookieJar: public QNetworkCookieJar
{
public:
    QnCustomCookieJar(QObject* parent = 0): QNetworkCookieJar(parent) {}

    void setCamera(QnResourcePtr camRes)
    {
        m_camRes = camRes;
    }

    virtual QList<QNetworkCookie> cookiesForUrl ( const QUrl & url ) const override 
    {
        QList<QNetworkCookie> result;
        if (m_camRes)
            result << QNetworkCookie("x-camera-guid", m_camRes->getId().toByteArray());
        return result;
    }
private:
    QnResourcePtr m_camRes;
};

CameraAdvancedSettingsWebPage::CameraAdvancedSettingsWebPage( QObject* parent )
:
    QWebPage( parent )
{
    m_cookieJar = new QnCustomCookieJar(this);
    networkAccessManager()->setCookieJar(m_cookieJar);
    //networkAccessManager()->cookieJar()->insertCookie(QNetworkCookie("x-camera-guid", "{123-123-123}"));
}

QString	CameraAdvancedSettingsWebPage::userAgentForUrl( const QUrl& /*url*/ ) const
{
    //this User-Agent is required for vista camera to use html/js page, not java applet
    return lit("Mozilla/5.0 (Windows; U; Windows NT based; en-US) AppleWebKit/534.34 (KHTML, like Gecko)  QtWeb Internet Browser/3.8.5 http://www.QtWeb.net");
}

void CameraAdvancedSettingsWebPage::setCamera(QnResourcePtr camRes)
{
    m_cookieJar->setCamera(camRes);
}
