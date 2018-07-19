#pragma once

#include "../event_ribbon.h"

#include <QtCore/QModelIndex>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>

#include <ui/style/helper.h>

#include <nx/utils/disconnect_helper.h>

class QScrollBar;
class QVariantAnimation;

namespace nx {
namespace client {
namespace desktop {

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

    Qt::ScrollBarPolicy scrollBarPolicy() const;
    void setScrollBarPolicy(Qt::ScrollBarPolicy value);

    bool live() const;
    void setLive(bool value);

    void setViewportMargins(int top, int bottom);

    int count() const;

    int unreadCount() const;
    QnNotificationLevel::Value highestUnreadImportance() const;

    void updateHover(bool hovered, const QPoint& mousePos);

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

    int indexOf(EventTile* tile) const;
    int indexAtPos(const QPoint& pos) const;

    // Creates a tile widget for a data model item using the following roles:
    //     Qt::UuidRole for unique id
    //     Qt::DisplayRole for title
    //     Qt::DecorationRole for icon
    //     Qn::TimestampTextRole for timestamp
    //     Qn::DescriptionTextRole for description
    //     Qt::ForegroundRole for titleColor
    EventTile* createTile(const QModelIndex& index);
    static void updateTile(EventTile* tile, const QModelIndex& index);

    bool shouldSetTileRead(const EventTile* tile) const;

    void debugCheckGeometries();
    void debugCheckVisibility();

private:
    EventRibbon* const q = nullptr;
    QAbstractListModel* m_model = nullptr;
    QScopedPointer<QnDisconnectHelper> m_modelConnections;

    QScrollBar* const m_scrollBar = nullptr;
    QWidget* const m_viewport = nullptr;

    QList<EventTile*> m_tiles;
    QHash<EventTile*, int> m_positions;
    int m_totalHeight = 0;

    QSet<EventTile*> m_visible;
    QHash<EventTile*, QnNotificationLevel::Value> m_unread;

    QPointer<EventTile> m_hoveredTile;

    // Maps animation object to item index. Duplicate indices are allowed.
    // Animation objects are owned by EventRibbon::Private object.
    // When stopped/finished they are destroyed and pointers removed from this hash.
    QHash<QVariantAnimation*, int> m_itemShiftAnimations;

    QMap<int, int> m_currentShifts; //< Maps item index to shift value. Sorted (!).

    Qt::ScrollBarPolicy m_scrollBarPolicy = Qt::ScrollBarAlwaysOn;

    bool m_showDefaultToolTips = false;
    bool m_previewsEnabled = true;
    bool m_footersEnabled = true;
    bool m_scrollBarRelevant = true;
    bool m_live = true;

    int m_topMargin = style::Metrics::kStandardPadding;
    int m_bottomMargin = style::Metrics::kStandardPadding;
};

} // namespace desktop
} // namespace client
} // namespace nx
