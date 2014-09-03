/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
#define CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H

#ifdef QT_WEBKITWIDGETS_LIB

#include <QWebPage>


class CameraAdvancedSettingsWebPage
:
    public QWebPage
{
public:
    CameraAdvancedSettingsWebPage( QObject* parent = 0 );

protected:
    //!Implementation of QWebPage::userAgentForUrl
    virtual QString	userAgentForUrl( const QUrl& url ) const override;
};

#endif

#endif  //CAMERA_ADVANCED_SETTINGS_WEB_PAGE_H
