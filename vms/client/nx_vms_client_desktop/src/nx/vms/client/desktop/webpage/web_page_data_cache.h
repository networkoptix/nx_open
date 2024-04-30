// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QUrl>
#include <QtGui/QImage>

#include "web_page_settings.h"

class QTimer;
class QWebEnginePage;

namespace nx::vms::client::desktop {

/**
 * Provides a way to find Web Page icons and metadata, downloads them if necessary.
 */
class WebPageDataCache: public QObject
{
    Q_OBJECT

public:
    static constexpr auto kLoadingTimeout = std::chrono::seconds(60);
    static constexpr auto kIconSize = QSize{20, 20};
    static inline const QString kNoPath = "";

public:
    WebPageDataCache(QObject* parent = nullptr);
    virtual ~WebPageDataCache() override;

    /** Finds the icon path associated with webPageUrl. */
    std::optional<QUrl> findIconPath(const QUrl& webPageUrl);

    /** Refreshes the data associated with webPageUrl. */
    void refresh(const QUrl& webPageUrl);

    /** Updates the icon associated with webPageUrl if there is no loaded icon. */
    Q_INVOKABLE void update(const QUrl& webPageUrl, const QImage& image);

signals:
    /** Emitted when the icon associated with webPageUrl changes. */
    void iconChanged(const QUrl& webPageUrl);

    /** Emitted when the webpage "windowSize" metadata parameter changes. */
    void windowSizeChanged(const QUrl& webPageUrl, const QSize& size);

private:
    void loadNext();
    void saveIcon(const QUrl& webPageUrl, const QImage& icon, const QImage& icon2x = {});

private:
    QMap<QUrl /*webPageUrl*/, WebPageSettings> m_settings;

    QList<QUrl> m_urlsToRefresh;
    std::unique_ptr<QWebEnginePage> m_webPage;
    bool m_isIconLoaded = false;
    bool m_isMetadataLoaded = false;
    QTimer* m_timer = nullptr;
};

} // namespace nx::vms::client::desktop
