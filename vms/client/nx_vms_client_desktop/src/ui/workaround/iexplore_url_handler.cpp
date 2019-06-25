#include "iexplore_url_handler.h"

#include <QtCore/QSettings>
#include <QtGui/QDesktopServices>

#include <nx/network/http/http_types.h>

QnIexploreUrlHandler::QnIexploreUrlHandler(QObject *parent):
    QObject(parent)
{
    QDesktopServices::setUrlHandler(nx::network::http::kUrlSchemeName, this, "handleUrl");
    QDesktopServices::setUrlHandler(nx::network::http::kSecureUrlSchemeName, this, "handleUrl");
}

QnIexploreUrlHandler::~QnIexploreUrlHandler()
{
    QDesktopServices::unsetUrlHandler(nx::network::http::kUrlSchemeName);
    QDesktopServices::unsetUrlHandler(nx::network::http::kSecureUrlSchemeName);
}

bool QnIexploreUrlHandler::handleUrl(QUrl url) {
    bool canAuth = !url.userName().isEmpty();

    if (canAuth) {
        QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\Clients\\StartMenuInternet"), QSettings::NativeFormat);
        QString defaultBrowser = settings.value(QLatin1String("Default")).toString().toLower();
        if (defaultBrowser.isEmpty() || defaultBrowser.contains(QLatin1String("iexplore")))
            canAuth = false;
    }
    if (!canAuth) {
        url.setUserName(QString());
        url.setPassword(QString());
    }

    return QDesktopServices::openUrl(url);
}
