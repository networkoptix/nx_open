/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
#define CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H

#ifdef GDM_TODO
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineProfile>

#include "core/resource/resource_fwd.h"

class CameraAdvancedSettingsWebPage
:
    public QWebEnginePage
{
public:
    CameraAdvancedSettingsWebPage(QWebEngineProfile *profile, QObject* parent = 0 );
    void setCamera(const QnResourcePtr &camera);
private:
    QNetworkCookieJar* m_cookieJar;
};
#endif

#endif  //CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
