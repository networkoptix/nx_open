#pragma once

#include <ui/graphics/items/generic/slider_tooltip_widget.h>

class GraphicsLabel;
class QnTimeSlider;

namespace nx {
namespace client {
namespace desktop {

class TimelineCursorLayout;

// Shows cursor callout and a vertical mark above the Timeline
class TimelineScreenshotCursor: public QnSliderTooltipWidget
{
    Q_OBJECT

    using base_type = QnSliderTooltipWidget;

 public:
    explicit TimelineScreenshotCursor(QnTimeSlider* slider,  QGraphicsItem* tooltipParent);
    virtual ~TimelineScreenshotCursor();

    void showAt(qreal position);

private:
    QnTimeSlider * m_slider;
    // Layout with time labels and a screenshot.
    TimelineCursorLayout* m_layout;
    // Vertical line running above the Timeline.
    QGraphicsLineItem*  m_mark;
};

} // namespace desktop
} // namespace client
} // namespace nx
