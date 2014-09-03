/**********************************************************
* 1 sep 2014
* a.kolesnikov
***********************************************************/

#ifdef QT_WEBKITWIDGETS_LIB

#include "camera_advanced_settings_web_page.h"


CameraAdvancedSettingsWebPage::CameraAdvancedSettingsWebPage( QObject* parent )
:
    QWebPage( parent )
{
}

QString	CameraAdvancedSettingsWebPage::userAgentForUrl( const QUrl& /*url*/ ) const
{
    //this User-Agent is required for vista camera to use html/js page, not java applet
    return lit("Mozilla/5.0 (Windows; U; Windows NT based; en-US) AppleWebKit/534.34 (KHTML, like Gecko)  QtWeb Internet Browser/3.8.5 http://www.QtWeb.net");
}

#endif
