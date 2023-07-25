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

    /** Updates the icon associated with webPageUrl if there is no loaded icon. */
    Q_INVOKABLE void update(const QUrl& webPageUrl, const QImage& image);

signals:
    /** Emitted when the icon associated with webPageUrl changes. */
    void iconChanged(const QUrl& webPageUrl);

private:
    void loadNext();
    void saveIcon(const QUrl& webPageUrl, const QImage& icon, const QImage& icon2x = {});

private:
    QMap<QUrl /*webPageUrl*/, QUrl /*iconUrl*/> m_iconPaths;
    QList<QUrl> m_urlsToRefresh;
    std::unique_ptr<QWebEnginePage> m_webPage;
    QTimer* m_timer = nullptr;
};

} // namespace nx::vms::client::desktop
