#include "timeline_screenshot_cursor.h"

#include <QtWidgets/QGraphicsProxyWidget>

#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/image_providers/image_provider.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/timeline/graphics/timeline_cursor_layout.h>
#include <utils/common/event_processors.h>
#include <core/resource/resource.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_navigator.h>

using std::chrono::milliseconds;
using std::chrono::microseconds;

namespace nx::vms::client::desktop {

namespace {
constexpr int kThumbnailWidth = 192;
constexpr int kThumbnailHeight = 108;
constexpr int kLazyUpdateMs = 1000; //< Autoupdate cursor once per sec.
} // namespace

TimelineScreenshotCursor::TimelineScreenshotCursor(QnTimeSlider* slider, QGraphicsItem* tooltipParent):
    base_type(slider),
    QnWorkbenchContextAware(slider),
    m_slider(slider),
    m_layout(new TimelineCursorLayout()),
    m_mark(new QGraphicsLineItem(this)),
    m_thumbnail(new AsyncImageWidget()),
    m_thumbnailHeight(kThumbnailHeight),
    m_lazyExecutor([this] { showNow(); }, kLazyUpdateMs)
 {
    setParentItem(tooltipParent);
    setContentsMargins(0.0, 0.0, 0.0, 0.0);

    m_thumbnail = new AsyncImageWidget();
    m_thumbnail->setMaximumWidth(kThumbnailWidth);
    m_thumbnail->setMinimumWidth(kThumbnailWidth);
    m_thumbnail->setReloadMode(AsyncImageWidget::ReloadMode::showPreviousImage);

    auto graphicsWidget = new QGraphicsProxyWidget();
    graphicsWidget->setWidget(m_thumbnail);

    auto mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
    mainLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    mainLayout->addItem(graphicsWidget);
    mainLayout->addItem(m_layout);
    setLayout(mainLayout);

    m_mark->setParentItem(slider);
    m_mark->setPen(palette().mid().color());

    hide();
}

TimelineScreenshotCursor::~TimelineScreenshotCursor()
{
    m_thumbnail->setImageProvider(nullptr);
}

void TimelineScreenshotCursor::showAt(qreal position, bool lazy)
{
    m_position = position;
    if (lazy) // Execute showNow(), but probably later and not too often.
        m_lazyExecutor.requestOperation();
    else
        showNow();
}

TimelineCursorLayout* TimelineScreenshotCursor::content()
{
    return m_layout;
}

QVariant TimelineScreenshotCursor::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemVisibleHasChanged)
        m_mark->setVisible(value.toBool());
    return QGraphicsItem::itemChange(change, value);
}

void TimelineScreenshotCursor::polishEvent()
{
    base_type::polishEvent();

    // We should set color for the mark on the timeline.
    m_mark->setPen(palette().mid().color());
}

void TimelineScreenshotCursor::showNow()
{
    const auto currentWidget = navigator()->currentWidget();
    if (!currentWidget)
        return;

    const auto currentResource = currentWidget->resource();
    NX_ASSERT(currentResource);
    if (!currentResource->hasFlags(Qn::media))
        return;

    milliseconds timePoint = m_slider->timeFromPosition(QPointF(m_position, 0), true);
    nx::api::ResourceImageRequest request;
    request.usecSinceEpoch = microseconds(timePoint).count();
    request.size = QSize(kThumbnailWidth, 0);
    request.imageFormat = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.roundMethod = nx::api::ImageRequest::RoundMethod::iFrameBefore;
    request.resource = currentResource;

    // We probably want to initialize image provider first time unconditionally.
    if (!m_imageProvider)
    {
        m_imageProvider.reset(new ResourceThumbnailProvider(request, this));
        m_thumbnail->setImageProvider(m_imageProvider.data());
        m_imageProvider->loadAsync();
    }
    else
        m_imageProvider->setRequestData(request);

    bool isLocalFile = currentResource->hasFlags(Qn::local_video)
        && !currentResource->hasFlags(Qn::periods);
    bool imageExists = isLocalFile
        || m_slider->timePeriods(0, Qn::RecordingContent).containTime(timePoint.count());

    m_thumbnail->setNoDataMode(!imageExists);

    if(imageExists)
        m_imageProvider->loadAsync(); //< Seems second call to loadAsync is OK.

    show();
    pointTo(mapToParent(mapFromItem(m_slider, m_position, -kToolTipMargin)));
    m_mark->setLine(m_position, 0, m_position, m_slider->geometry().height());
}

} // namespace nx::vms::client::desktop

