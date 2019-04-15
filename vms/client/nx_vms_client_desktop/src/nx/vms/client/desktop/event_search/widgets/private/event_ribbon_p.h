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
#include <QtCore/QVariantAnimation>
#include <QtCore/QHash>

#include <ui/common/notification_levels.h>
#include <ui/style/helper.h>

#include <nx/utils/interval.h>
#include <nx/utils/pending_operation.h>
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

    void setViewportMargins(int top, int bottom);

    QWidget* viewportHeader() const;
    void setViewportHeader(QWidget* value); //< Takes ownership.

    int count() const;

    int unreadCount() const;
    QnNotificationLevel::Value highestUnreadImportance() const;

    nx::utils::Interval<int> visibleRange() const;

    void updateHover();

    void setInsertionMode(UpdateMode updateMode, bool scrollDown);
    void setRemovalMode(UpdateMode updateMode);

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    void updateView(); //< Calls doUpdateView and emits q->unreadCountChanged if needed.
    void doUpdateView();
    void updateScrollRange();

    int calculateHeight(QWidget* widget) const;

    void insertNewTiles(int index, int count, UpdateMode updateMode, bool scrollDown);
    void removeTiles(int first, int count, UpdateMode updateMode);
    void clear();

    void fadeIn(EventTile* widget);
    void fadeOut(EventTile* widget);
    QWidget* createFadeCurtain(EventTile* widget, QVariantAnimation* animator);

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

    void closeExpiredTiles();

    void loadNextPreview();

    int scrollValue() const;
    int totalTopMargin() const; //< Top margin and viewport header.

    // Top of the viewport in tile position coordinates.
    int viewportTopPosition() const { return scrollValue() - totalTopMargin(); }

    // Calculate specified tile position without animated shift, based on previous tile position
    // and animated height.
    int calculatePosition(int index) const;

private:
    EventRibbon* const q = nullptr;
    QAbstractListModel* m_model = nullptr;
    nx::utils::ScopedConnections m_modelConnections;
    const std::unique_ptr<QScrollBar> m_scrollBar;
    const std::unique_ptr<QWidget> m_viewport;
    const std::unique_ptr<QTimer> m_autoCloseTimer;

    using Importance = QnNotificationLevel::Value;
    static constexpr int kApproximateTileHeight = 48;

    using AnimationPtr = std::unique_ptr<QVariantAnimation>;

    struct Tile
    {
        int height = kApproximateTileHeight;
        int position = 0; //< Positions start from 0, without top margin & viewport header.
        Importance importance = Importance();
        bool animated = false;
        std::unique_ptr<ResourceThumbnailProvider> preview;
        std::unique_ptr<EventTile> widget;

        AnimationPtr insertAnimation;
        AnimationPtr removeAnimation;

        int animatedHeight() const
        {
            return insertAnimation
                ? int(insertAnimation->currentValue().toReal() * height)
                : height;
        }

        int animatedShift() const
        {
            return removeAnimation ? removeAnimation->currentValue().toInt() : 0;
        }
    };

    using TilePtr = std::unique_ptr<Tile>;
    using Interval = nx::utils::Interval<int>;

    std::deque<TilePtr> m_tiles;
    std::stack<std::unique_ptr<EventTile>> m_reserveWidgets;
    QPersistentModelIndex m_hoveredIndex;
    bool m_tileHovered = false;
    Interval m_visible;

    QHash<QPersistentModelIndex, QDeadlineTimer> m_deadlines;

    QHash<QVariantAnimation*, QPersistentModelIndex> m_animations;

    std::array<int, int(Importance::LevelCount)> m_unreadCounts{};
    int m_totalUnreadCount = 0;

    nx::utils::Guard makeUnreadCountGuard();

    int m_endPosition = 0;
    AnimationPtr m_endAnimation;

    Qt::ScrollBarPolicy m_scrollBarPolicy = Qt::ScrollBarAlwaysOn;

    bool m_showDefaultToolTips = false;
    bool m_previewsEnabled = true;
    bool m_footersEnabled = true;
    bool m_headersEnabled = true;
    bool m_scrollBarRelevant = true;
    bool m_updating = false;

    UpdateMode m_insertionMode = UpdateMode::animated;
    UpdateMode m_removalMode = UpdateMode::animated;
    bool m_scrollDownAfterInsertion = false;

    QPointer<QWidget> m_viewportHeader;

    std::chrono::microseconds m_highlightedTimestamp{0};
    QSet<QnResourcePtr> m_highlightedResources;

    int m_topMargin = style::Metrics::kStandardPadding;
    int m_bottomMargin = style::Metrics::kStandardPadding;

    nx::utils::ImplPtr<nx::utils::PendingOperation> m_previewLoad;
};

} // namespace nx::vms::client::desktop
