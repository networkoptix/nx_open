#pragma once

#include "../event_ribbon.h"

#include <nx/utils/integer_range.h>

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

    void insertTile(int index, EventTile* tileWidget);
    void removeTiles(int first, int count);

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

private:
    EventRibbon* q = nullptr;

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
