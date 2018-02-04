#pragma once

#include "../event_ribbon.h"

#include <QtCore/QModelIndex>
#include <QtCore/QScopedPointer>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QSet>

#include <nx/utils/disconnect_helper.h>

class QScrollBar;
class QVariantAnimation;

namespace nx {
namespace client {
namespace desktop {

class EventTile;

class EventRibbon::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventRibbon* q);
    virtual ~Private() override;

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* model);

    int totalHeight() const;

    QScrollBar* scrollBar() const;

    int count() const;

    int unreadCount() const;
    QnNotificationLevel::Value highestUnreadImportance() const;

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

    int indexOf(EventTile* tile) const;

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

    // Maps animation object to item index. Duplicate indices are allowed.
    // Animation objects are owned by EventRibbon::Private object.
    // When stopped/finished they are destroyed and pointers removed from this hash.
    QHash<QVariantAnimation*, int> m_itemShiftAnimations;

    QHash<int, int> m_currentShifts; //< Maps item index to shift value.
};

} // namespace desktop
} // namespace client
} // namespace nx
