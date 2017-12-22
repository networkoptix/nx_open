#pragma once

#include "../event_ribbon.h"

#include <QtCore/QModelIndex>
#include <QtCore/QScopedPointer>

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

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* model);

    int totalHeight() const;

    QScrollBar* scrollBar() const;

private:
    void updateView();
    void updateScrollRange();

    int calculateHeight(QWidget* widget) const;

    void insertNewTiles(int index, int count);
    void removeTiles(int first, int count);
    void clear();

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
};

} // namespace
} // namespace client
} // namespace nx
