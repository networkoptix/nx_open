#include "iexplore_url_handler.h"

#include <QDesktopServices>

QnIexploreUrlHandler::QnIexploreUrlHandler(): QObject()
{
    QDesktopServices::setUrlHandler(QString(QLatin1String("http")), this, SLOT(handleUrl(QUrl)));
    QDesktopServices::setUrlHandler(QString(QLatin1String("https")), this, SLOT(handleUrl(QUrl)));
}

QnIexploreUrlHandler::~QnIexploreUrlHandler(){
    QDesktopServices::unsetUrlHandler(QString(QLatin1String("http")));
    QDesktopServices::unsetUrlHandler(QString(QLatin1String("https")));
}

bool QnIexploreUrlHandler::handleUrl(QUrl url){

    bool canAuth = !url.userName().isEmpty();

    if (canAuth){
        QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\Clients\\StartMenuInternet"), QSettings::NativeFormat);
        QString defaultBrowser = settings.value(QLatin1String("Default")).toString().toLower();
        if (defaultBrowser.isEmpty() || defaultBrowser.contains(QLatin1String("iexplore")))
            canAuth = false;
    }
    if (!canAuth){
        url.setUserName(QString());
        url.setPassword(QString());
    }

    return QDesktopServices::openUrl(url);
}
