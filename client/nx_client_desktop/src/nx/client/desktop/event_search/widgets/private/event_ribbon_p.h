#pragma once

#include "../event_ribbon.h"

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

    void addTile(EventTile* tileWidget, const QnUuid& id);
    void removeTile(const QnUuid& id);

    int totalHeight() const;

    QScrollBar* scrollBar() const;

private:
    struct Tile
    {
        EventTile* widget = nullptr;
        QnUuid id;
        int position = 0;

        Tile() = default;
        Tile(EventTile* widget, const QnUuid& id): widget(widget), id(id) {}
    };

    void updateView();
    void updateScrollRange();
    void updateAllPositions();

    int calculateHeight(QWidget* widget) const;

private:
    EventRibbon* q = nullptr;

    QScrollBar* const m_scrollBar = nullptr;
    QWidget* const m_viewport = nullptr;

    QList<Tile> m_tiles;
    int m_totalHeight = 0;
    int m_currentWidth = 0;

    QSet<const Tile*> m_visibleTiles;
    int m_position = 0;
    int m_height = 0;
};

} // namespace
} // namespace client
} // namespace nx
