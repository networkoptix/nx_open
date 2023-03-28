// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QUrl>

class QTimer;
class QWebEnginePage;

namespace nx::vms::client::desktop {

/**
 * Provides a way to find Web Page icons, and downloads them if necessary.
 */
class WebPageIconCache: public QObject
{
    Q_OBJECT

public:
    static constexpr auto kLoadingTimeout = std::chrono::seconds(60);
    static constexpr auto kIconSize = QSize{20, 20};
    static inline const QString kNoPath = "";

public:
    WebPageIconCache(QObject* parent = nullptr);
    virtual ~WebPageIconCache() override;

    /** Finds the icon path associated with webPageUrl. */
    std::optional<QUrl> findPath(const QUrl& webPageUrl);

    /** Refreshes the icon associated with webPageUrl. */
    void refresh(const QUrl& webPageUrl);

signals:
    /** Emitted when the icon associated with webPageUrl changes. */
    void iconChanged(const QUrl& webPageUrl);

private:
    void loadNext();
    void iconLoaded(const QUrl& webPageUrl);

private:
    QMap<QUrl /*webPageUrl*/, QUrl /*iconUrl*/> m_iconPaths;
    QList<QUrl> m_urlsToRefresh;
    std::unique_ptr<QWebEnginePage> m_webPage;
    QTimer* m_timer = nullptr;
};

} // namespace nx::vms::client::desktop
