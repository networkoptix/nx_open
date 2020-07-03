#pragma once

#include <nx/utils/uuid.h>
#include <ui/graphics/items/standard/graphics_widget.h>

namespace nx::vms::client::desktop {

class AreaHighlightOverlayWidget: public GraphicsWidget
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    struct AreaInformation
    {
        QnUuid id;
        QRectF rectangle;
        QColor color;
        QString text;
        QString hoverText;

        /**
         * API allows to send only text info, assigned to a certain point. This is implemented by
         * sending a null rectangle, which top left pos is used as a point coordinates.
         */
        bool isPoint() { return rectangle.isNull(); }
    };

    AreaHighlightOverlayWidget(QGraphicsWidget* parent);
    virtual ~AreaHighlightOverlayWidget() override;

    void addOrUpdateArea(const AreaInformation& areaInformation);
    void removeArea(const QnUuid& areaId);

    QnUuid highlightedArea() const;
    void setHighlightedArea(const QnUuid& areaId);

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* w) override;
    virtual bool event(QEvent* event) override;

signals:
    void highlightedAreaChanged(const QnUuid& areaId);
    void areaClicked(const QnUuid& areaId);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
