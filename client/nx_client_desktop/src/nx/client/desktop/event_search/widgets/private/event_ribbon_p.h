#pragma once

#include "../event_ribbon.h"

#include <QtCore/QModelIndex>
#include <QtCore/QScopedPointer>

#include <nx/utils/integer_range.h>
#include <nx/utils/disconnect_helper.h>

class QScrollBar;

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

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    int totalHeight() const;

    QScrollBar* scrollBar() const;

private:
    struct Tile
    {
        EventTile* widget = nullptr;
        int position = 0;

        Tile() = default;
        Tile(EventTile* widget, int position = 0): widget(widget), position(position) {}
    };

    void updateView();
    void updateScrollRange();
    void updateAllPositions();

    int calculateHeight(QWidget* widget) const;

    void insertTile(int index, EventTile* tileWidget);
    void insertNewTiles(int first, int count);
    void removeTiles(int first, int count);
    void clear();

    // Creates a tile widget for a data model item using the following roles:
    //     Qt::UuidRole for unique id
    //     Qt::DisplayRole for title
    //     Qt::DecorationRole for icon
    //     Qn::TimestampTextRole for timestamp
    //     Qn::DescriptionTextRole for description
    //     Qt::ForegroundRole for titleColor
    EventTile* createTile(const QModelIndex& index);
    static void updateTile(EventTile* tile, const QModelIndex& index);

private:
    EventRibbon* const q = nullptr;
    QAbstractItemModel* m_model = nullptr;
    QScopedPointer<QnDisconnectHelper> m_modelConnections;

    // These fields are constant for now.
    // If required in the future they can be made changeable from public interface.
    QModelIndex m_rootIndex;
    int m_modelColumn = 0;

    QScrollBar* const m_scrollBar = nullptr;
    QWidget* const m_viewport = nullptr;

    QList<Tile> m_tiles;
    int m_totalHeight = 0;
    int m_currentWidth = 0;

    nx::utils::IntegerRange m_visible;
};

} // namespace
} // namespace client
} // namespace nx
