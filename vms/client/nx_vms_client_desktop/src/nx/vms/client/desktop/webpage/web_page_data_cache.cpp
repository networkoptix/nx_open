// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_page_data_cache.h"

#include <QtCore/QDir>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <QtWebEngineCore/QWebEnginePage>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

static QDir makeCachePath()
{
    const QDir path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QDir::toNativeSeparators("/cache/web_page_icons/");

    path.mkpath(".");
    return path;
}

static QString iconFileName(const QUrl& webPageUrl, const QString& postfix = {})
{
    return nx::utils::replaceNonFileNameCharacters(webPageUrl.host() + webPageUrl.path(), '_')
        + postfix + ".png";
}

void loadMetadata(
    QWebEnginePage* webPage,
    std::function<void(bool hasIcon, bool dedicatedWindow, QSize windowSize)> callback)
{
    const QString getMetadata = R"(
        const dedicatedWindow = document.querySelector("meta[name='dedicatedWindow']")
        const icon = document.querySelector("link[rel*='icon']")

        // Result expression.
        new Object({
            hasIcon: !!icon,
            dedicatedWindow: dedicatedWindow && {
                width: parseInt(dedicatedWindow.attributes.width?.value) || 0,
                height: parseInt(dedicatedWindow.attributes.height?.value) || 0
            }
        })
    )";

    webPage->runJavaScript(
        getMetadata,
        nx::utils::guarded(webPage,
            [callback](const QVariant& value)
            {
                const QJsonObject result = value.toJsonObject();
                const QJsonObject dedicatedWindow = result["dedicatedWindow"].toObject();
                const QSize windowSize =
                    QSize{dedicatedWindow["width"].toInt(), dedicatedWindow["height"].toInt()};

                callback(result["hasIcon"].toBool(), !dedicatedWindow.isEmpty(), windowSize);
            }));
}

} // namespace

WebPageDataCache::WebPageDataCache(QObject* parent):
    QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kLoadingTimeout);
    m_timer->callOnTimeout(this, &WebPageDataCache::loadNext);

    for (const auto& [webPageUrl, webPageSettings]:
        appContext()->localSettings()->webPageSettings().asKeyValueRange())
    {
        m_iconPaths[webPageUrl] = webPageSettings.icon.value_or("");
    }
}

WebPageDataCache::~WebPageDataCache()
{
    QMap<QString, WebPageSettings> settings = appContext()->localSettings()->webPageSettings();
    for (const auto [webPageUrl, iconUrl]: m_iconPaths.asKeyValueRange())
        settings[webPageUrl.toString()].icon = iconUrl.toString();

    appContext()->localSettings()->webPageSettings = settings;
}

void WebPageDataCache::loadNext()
{
    if (!m_urlsToRefresh.empty())
    {
        const QUrl webPageUrl = m_urlsToRefresh.takeFirst();

        m_webPage = std::make_unique<QWebEnginePage>();
        m_isMetadataLoaded = false;
        m_isIconLoaded = false;

        connect(m_webPage.get(), &QWebEnginePage::iconChanged, this,
            [this, webPageUrl]()
            {
                const QIcon icon = m_webPage->icon();

                saveIcon(
                    webPageUrl,
                    icon.pixmap(kIconSize).toImage(),
                    icon.pixmap(kIconSize * 2).toImage());

                // Wait until both an icon and metadata are loaded.
                m_isIconLoaded = true;
                if (m_isMetadataLoaded)
                    executeLater([this]() { loadNext(); }, this);
            });

        connect(m_webPage.get(), &QWebEnginePage::loadFinished, m_webPage.get(),
            [this, webPageUrl](bool ok)
            {
                if (!ok || !m_webPage)
                {
                    executeLater([this]() { loadNext(); }, this);
                    return;
                }

                loadMetadata(m_webPage.get(),
                    [this, webPageUrl](bool hasIcon, bool dedicatedWindow, QSize windowSize)
                    {
                        if (dedicatedWindow)
                            emit dedicatedWindowSettingsLoaded(webPageUrl, windowSize);

                        // Wait until both an icon and metadata are loaded.
                        m_isMetadataLoaded = true;
                        if (m_isIconLoaded || !hasIcon)
                            executeLater([this]() { loadNext(); }, this);
                    });
            });

        m_webPage->setUrl(webPageUrl);
        m_timer->start();
    }
    else
    {
        m_webPage.reset();
    }
}

void WebPageDataCache::saveIcon(const QUrl& webPageUrl, const QImage& icon, const QImage& icon2x)
{
    const QDir cachePath = makeCachePath();
    const QString path = cachePath.filePath(iconFileName(webPageUrl));
    const QString path2x = cachePath.filePath(iconFileName(webPageUrl, "@2x"));

    if (!icon.save(path))
    {
        NX_DEBUG(this, "Failed to save the icon for %1 as %2", webPageUrl, path);
        return;
    }

    if (!icon2x.isNull())
        icon2x.save(path2x);

    m_iconPaths[webPageUrl] = QUrl::fromLocalFile(path).toString();
    emit iconChanged(webPageUrl);
}

std::optional<QUrl> WebPageDataCache::findIconPath(const QUrl& webPageUrl)
{
    if (!m_iconPaths.contains(webPageUrl))
        refresh(webPageUrl);

    const QUrl path = m_iconPaths.value(webPageUrl);

    if (!path.isEmpty() && !QFileInfo(path.toLocalFile()).exists())
    {
        refresh(webPageUrl);
        return std::nullopt;
    }

    return path.isEmpty() ? std::optional<QUrl>{} : path;
}

void WebPageDataCache::refresh(const QUrl& webPageUrl)
{
    if (!m_urlsToRefresh.contains(webPageUrl))
    {
        m_iconPaths[webPageUrl] = "";
        m_urlsToRefresh.append(webPageUrl);
        if (!m_webPage)
            loadNext();
    }
}

void WebPageDataCache::update(const QUrl& webPageUrl, const QImage& icon)
{
    if (webPageUrl.isEmpty() || m_iconPaths[webPageUrl] != kNoPath || icon.isNull())
        return;

    saveIcon(webPageUrl, icon.scaled(kIconSize), icon.scaled(kIconSize * 2));
}

} // namespace nx::vms::client::desktop
