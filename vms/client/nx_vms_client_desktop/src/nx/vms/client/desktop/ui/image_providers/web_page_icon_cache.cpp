// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_page_icon_cache.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <QtWebEngineCore/QWebEnginePage>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

namespace nx::vms::client::desktop {

namespace {

static QDir makeCachePath()
{
    const QDir path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QDir::toNativeSeparators("/cache/web_page_icons/");

    path.mkpath(".");
    return path;
}

} // namespace

WebPageIconCache::WebPageIconCache(QObject* parent):
    QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kLoadingTimeout);
    m_timer->callOnTimeout(this, &WebPageIconCache::loadNext);

    for (const auto [webPageUrl, iconUrl]: appContext()->localSettings()->webPageIcons())
        m_iconPaths[webPageUrl] = iconUrl;
}

WebPageIconCache::~WebPageIconCache()
{
    std::map<QString, QString> iconPaths;
    for (const auto [webPageUrl, iconUrl]: m_iconPaths.asKeyValueRange())
        iconPaths[webPageUrl.toString()] = iconUrl.toString();

    appContext()->localSettings()->webPageIcons = iconPaths;
}

void WebPageIconCache::loadNext()
{
    if (!m_urlsToRefresh.empty())
    {
        const QUrl webPageUrl = m_urlsToRefresh.takeFirst();

        m_webPage = std::make_unique<QWebEnginePage>();
        connect(m_webPage.get(), &QWebEnginePage::iconChanged,
            this, [this, webPageUrl] { iconLoaded(webPageUrl); });

        m_webPage->setUrl(webPageUrl);
        m_timer->start();
    }
    else
    {
        m_webPage.reset();
    }
}

void WebPageIconCache::iconLoaded(const QUrl& webPageUrl)
{
    const QDir cachePath = makeCachePath();
    const QString fileName = webPageUrl.host() + "_" + m_webPage->iconUrl().fileName();
    const QString path = cachePath.filePath(fileName);

    if (m_webPage->icon().pixmap(kIconSize).save(path))
    {
        m_iconPaths[webPageUrl] = QUrl::fromLocalFile(path);
        emit iconChanged(webPageUrl);
    }

    loadNext();
}

std::optional<QUrl> WebPageIconCache::findPath(const QUrl& webPageUrl)
{
    if (!m_iconPaths.contains(webPageUrl))
        refresh(webPageUrl);

    const QUrl path = m_iconPaths[webPageUrl];

    if (!path.isEmpty() && !QFileInfo(path.toLocalFile()).exists())
    {
        refresh(webPageUrl);
        return std::nullopt;
    }

    return path.isEmpty() ? std::optional<QUrl>{} : path;
}

void WebPageIconCache::refresh(const QUrl& webPageUrl)
{
    if (!m_urlsToRefresh.contains(webPageUrl))
    {
        m_iconPaths[webPageUrl] = kNoPath;
        m_urlsToRefresh.append(webPageUrl);
        if (!m_webPage)
            loadNext();
    }
}

} // namespace nx::vms::client::desktop
