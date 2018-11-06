#pragma once

#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/pending_operation.h>

class QnTimeSlider;

namespace nx::vms::client::desktop {

class AsyncImageWidget;
class TimelineCursorLayout;
class ResourceThumbnailProvider;

// Shows cursor callout and a vertical mark above the Timeline.
class TimelineScreenshotCursor: public QnSliderTooltipWidget, QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QnSliderTooltipWidget;

 public:
    explicit TimelineScreenshotCursor(QnTimeSlider* slider,  QGraphicsItem* tooltipParent);
    virtual ~TimelineScreenshotCursor();

    void showAt(qreal position, bool lazy = false);

    TimelineCursorLayout* content();

    // We overwrite ItemVisibleHasChanged to hide or show Timeline mark.
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

protected:
    virtual void polishEvent() override;

private:
    QnTimeSlider* m_slider;
    // Layout with time labels and a screenshot.
    TimelineCursorLayout* m_layout;
    // Vertical line running above the Timeline.
    QGraphicsLineItem*  m_mark;
    // Thumbnail object.
    AsyncImageWidget* m_thumbnail;
    // Thumbnail image provider. Will be deleted safely after destructor.
    QScopedPointer<ResourceThumbnailProvider> m_imageProvider;

    int m_thumbnailHeight;
    utils::PendingOperation m_lazyExecutor;

    qreal m_position = 0;

    void showNow();
};

} // namespace nx::vms::client::desktop
