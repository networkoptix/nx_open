/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
#define CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H

#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineProfile>

#include "core/resource/resource_fwd.h"

class QnCustomCookieJar;

class CameraAdvancedSettingsWebPage
:
    public QWebEnginePage
{
public:
    CameraAdvancedSettingsWebPage(QWebEngineProfile *profile, QObject* parent = 0 );
    void setCamera(QnResourcePtr camRes);
private:
    QnCustomCookieJar* m_cookieJar;
};

#endif  //CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
