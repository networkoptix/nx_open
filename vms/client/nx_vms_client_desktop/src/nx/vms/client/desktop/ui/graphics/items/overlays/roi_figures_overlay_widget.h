#pragma once

#include <ui/graphics/items/standard/graphics_widget.h>

class QnResourceWidget;

namespace nx::vms::client::desktop {

class RoiFiguresOverlayWidget: public GraphicsWidget
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    struct Item
    {
        QVector<QPointF> points;
        QColor color;

        bool isValid() const { return !points.isEmpty(); }
    };

    struct Line: Item
    {
        enum class Direction { none, a, b };
        Direction direction = Direction::none;
    };

    struct Box: Item
    {
    };

    struct Polygon: Item
    {
    };

    RoiFiguresOverlayWidget(QGraphicsWidget* parent, QnResourceWidget* resourceWidget);
    virtual ~RoiFiguresOverlayWidget() override;

    void addLine(const QString& id, const Line& line);
    void removeLine(const QString& id);
    void addBox(const QString& id, const Box& box);
    void removeBox(const QString& id);
    void addPolygon(const QString& id, const Polygon& polygon);
    void removePolygon(const QString& id);

    void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
