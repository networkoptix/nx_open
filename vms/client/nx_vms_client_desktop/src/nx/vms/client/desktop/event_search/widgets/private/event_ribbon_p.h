#pragma once

#include "../event_ribbon.h"

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <stack>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QHash>
#include <QtCore/QMap>

#include <ui/common/notification_levels.h>
#include <ui/style/helper.h>

#include <nx/utils/interval.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>

class QScrollBar;
class QVariantAnimation;

namespace nx::vms::client::desktop {

class EventRibbon::Private: public QObject
{
    Q_OBJECT
    using PrivateSignal = EventRibbon::QPrivateSignal;

public:
    Private(EventRibbon* q);
    virtual ~Private() override;

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* model);

    int totalHeight() const;

    QScrollBar* scrollBar() const;

    bool showDefaultToolTips() const;
    void setShowDefaultToolTips(bool value);

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool value);

    bool footersEnabled() const;
    void setFootersEnabled(bool value);

    bool headersEnabled() const;
    void setHeadersEnabled(bool value);

    Qt::ScrollBarPolicy scrollBarPolicy() const;
    void setScrollBarPolicy(Qt::ScrollBarPolicy value);

    std::chrono::microseconds highlightedTimestamp() const;
    void setHighlightedTimestamp(std::chrono::microseconds value);

    QSet<QnResourcePtr> highlightedResources() const;
    void setHighlightedResources(const QSet<QnResourcePtr>& value);

    bool live() const;
    void setLive(bool value);

    void setViewportMargins(int top, int bottom);

    QWidget* viewportHeader() const;
    void setViewportHeader(QWidget* value); //< Takes ownership.

    int count() const;

    int unreadCount() const;
    QnNotificationLevel::Value highestUnreadImportance() const;

    nx::utils::Interval<int> visibleRange() const;

    void updateHover();

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    void updateView(); //< Calls doUpdateView and emits q->unreadCountChanged if needed.
    void doUpdateView();
    void updateScrollRange();

    int calculateHeight(QWidget* widget) const;

    enum class UpdateMode
    {
        instant,
        animated
    };

    void insertNewTiles(int index, int count, UpdateMode updateMode);
    void removeTiles(int first, int count, UpdateMode updateMode);
    void clear();

    void highlightAppearance(EventTile* tile);
    void addAnimatedShift(int index, int shift); //< Animates from shift to zero.
    void updateCurrentShifts(); //< Updates m_currentShifts from m_itemShiftAnimations.
    void clearShiftAnimations();

    void showContextMenu(EventTile* tile, const QPoint& posRelativeToTile);

    void setScrollBarRelevant(bool value);
    void updateScrollBarVisibility();

    void updateHighlightedTiles(); //< By highlighted timestamp, not appearing animation.

    int indexOf(const EventTile* widget) const;
    int indexAtPos(const QPoint& pos) const;

    void updateTile(int index);
    void updateTilePreview(int index);
    void ensureWidget(int index);
    void reserveWidget(int index);

    void handleItemAboutToBeRemoved(int index);
    void handleWidgetChanged(int index);
    void closeExpiredTiles();

private:
    EventRibbon* const q = nullptr;
    QAbstractListModel* m_model = nullptr;
    nx::utils::ScopedConnections m_modelConnections;
    const std::unique_ptr<QScrollBar> m_scrollBar;
    const std::unique_ptr<QWidget> m_viewport;
    const std::unique_ptr<QTimer> m_autoCloseTimer;

    using Importance = QnNotificationLevel::Value;
    static constexpr int kApproximateTileHeight = 48;

    struct Tile
    {
        int height = kApproximateTileHeight;
        int position = 0;
        Importance importance = Importance();
        bool animated = false;
        std::unique_ptr<ResourceThumbnailProvider> preview;
        std::unique_ptr<EventTile> widget;
    };

    using TilePtr = std::unique_ptr<Tile>;
    using Interval = nx::utils::Interval<int>;

    std::deque<TilePtr> m_tiles;
    std::stack<std::unique_ptr<EventTile>> m_reserveWidgets;
    QPersistentModelIndex m_hoveredIndex;
    Interval m_visible;

    QHash<QPersistentModelIndex, QDeadlineTimer> m_deadlines;

    std::array<int, int(Importance::LevelCount)> m_unreadCounts{};
    int m_totalUnreadCount = 0;

    nx::utils::Guard makeUnreadCountGuard();

    int m_totalHeight = 0;

    // Maps animation object to item index. Duplicate indices are allowed.
    // Animation objects are owned by EventRibbon::Private object.
    // When stopped/finished they are destroyed and pointers removed from this hash.
    QHash<QVariantAnimation*, int> m_itemShiftAnimations;

    QMap<int, int> m_currentShifts; //< Maps item index to shift value. Sorted (!).

    Qt::ScrollBarPolicy m_scrollBarPolicy = Qt::ScrollBarAlwaysOn;

    bool m_showDefaultToolTips = false;
    bool m_previewsEnabled = true;
    bool m_footersEnabled = true;
    bool m_headersEnabled = true;
    bool m_scrollBarRelevant = true;
    bool m_live = true;
    bool m_updating = false;

    QPointer<QWidget> m_viewportHeader;

    std::chrono::microseconds m_highlightedTimestamp{0};
    QSet<QnResourcePtr> m_highlightedResources;

    int m_topMargin = style::Metrics::kStandardPadding;
    int m_bottomMargin = style::Metrics::kStandardPadding;
};

} // namespace nx::vms::client::desktop
