
#include "workbench_item_bookmarks_watcher.h"

#include <utils/common/string.h>
#include <utils/camera/bookmark_helpers.h>
#include <camera/camera_bookmark_aggregation.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>
#include <ui/utils/workbench_item_helper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/graphics/items/overlays/text_overlay_widget.h>
#include <ui/graphics/items/overlays/composite_text_overlay.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace
{
    enum
    {
        kWindowWidthMs =  5 * 60 * 1000      // 3 minute before and after current position
        , kLeftOffset = kWindowWidthMs / 2
        , kRightOffset = kLeftOffset

        , kMinWindowChange = 10000
    };

    const QnCameraBookmarkSearchFilter kInitialFilter = []()
    {
        enum { kMaxBookmarksNearThePosition = 256 };

        QnCameraBookmarkSearchFilter filter;
        filter.sparsing.used = true;
        filter.limit = kMaxBookmarksNearThePosition;
        filter.startTimeMs = DATETIME_INVALID;
        filter.endTimeMs = DATETIME_INVALID;
        return filter;
    }();

    QnOverlayTextItemData makeBookmarkItem(const QnCameraBookmark &bookmark
        , const QnBookmarkColors &colors)
    {
        enum
        {
            kBorderRadius = 4
            , kPadding = 8

            , kCaptionMaxLength = 64
            , kDescriptionMaxLength = 160
            , kMaxItemWidth = 250
        };

        static QnHtmlTextItemOptions options = QnHtmlTextItemOptions(
            colors.background, false, kBorderRadius, kPadding, kPadding, kMaxItemWidth);

        enum
        {
            kCaptionPixelSize = 16
            , kDescriptionPixeSize = 12
        };

        const auto captionHtml = elideHtml(htmlFormattedParagraph(
            bookmark.name, kCaptionPixelSize, true), kCaptionMaxLength);
        const auto descHtml = elideHtml(htmlFormattedParagraph(
            bookmark.description, kDescriptionPixeSize), kDescriptionMaxLength);

        static const auto kHtmlPageTemplate = lit("<html><head><style>* {text-ident: 0; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; color: white;}</style></head><body>%1</body></html>");
        static const auto kComplexHtml = lit("%1%2");

        const auto bookmarkHtml = kHtmlPageTemplate.arg(kComplexHtml.arg(captionHtml, descHtml));
        return QnOverlayTextItemData(bookmark.guid, bookmarkHtml, options);
    };
}

//

class QnWorkbenchItemBookmarksWatcher::WidgetData : public Connective<QObject>
{
    typedef Connective<QObject> base_type;

    typedef QPointer<QnMediaResourceWidget> QnMediaResourceWidgetPtr;
    typedef QPointer<QnTimelineBookmarksWatcher> QnTimelineBookmarksWatcherPtr;
public:
    static WidgetDataPtr create(QnMediaResourceWidget *widget
        , QnWorkbenchContext *context
        , QnWorkbenchItemBookmarksWatcher *watcher);

    WidgetData(const QnVirtualCameraResourcePtr &camera
        , QnMediaResourceWidget *resourceWidget
        , QnTimelineBookmarksWatcher *timelineWatcher
        , QnWorkbenchItemBookmarksWatcher *watcher);

    ~WidgetData();

private:
    void updatePos(qint64 posMs);

    void updateQueryFilter();

    void updateBookmarksAtPosition();

    void updateBookmarks(const QnCameraBookmarkList &newBookmarks);

    void sendBookmarksToCompositeOverlay();

private:
    QnWorkbenchItemBookmarksWatcher * const m_parent;
    QnTimelineBookmarksWatcherPtr m_timelineWatcher;
    const QnMediaResourceWidgetPtr m_mediaWidget;

    qint64 m_posMs;
    QnCameraBookmarkList m_bookmarks;
    QnCameraBookmarkAggregation m_bookmarksAtPos;
    QnCameraBookmarksQueryPtr m_query;
};

QnWorkbenchItemBookmarksWatcher::WidgetDataPtr QnWorkbenchItemBookmarksWatcher::WidgetData::create(
    QnMediaResourceWidget *widget
    , QnWorkbenchContext *context
    , QnWorkbenchItemBookmarksWatcher *parent)
{
    if (!widget|| !context || !parent)
        return WidgetDataPtr();

    const auto camera = helpers::extractCameraResource(widget->item());
    if (!camera)
        return WidgetDataPtr();

    const auto timelineWatcher = context->instance<QnTimelineBookmarksWatcher>();

    return WidgetDataPtr(new WidgetData(camera, widget, timelineWatcher, parent));
}

QnWorkbenchItemBookmarksWatcher::WidgetData::WidgetData(const QnVirtualCameraResourcePtr &camera
    , QnMediaResourceWidget *resourceWidget
    , QnTimelineBookmarksWatcher *timelineWatcher
    , QnWorkbenchItemBookmarksWatcher *parent)
    : base_type(parent)
    , m_parent(parent)
    , m_timelineWatcher(timelineWatcher)
    , m_mediaWidget(resourceWidget)
    , m_posMs(DATETIME_INVALID)
    , m_bookmarks()
    , m_bookmarksAtPos()
    , m_query(qnCameraBookmarksManager->createQuery(QnVirtualCameraResourceSet() << camera))
{
    m_query->setFilter(kInitialFilter);
    connect(m_query, &QnCameraBookmarksQuery::bookmarksChanged
        , this, &WidgetData::updateBookmarks);

    connect(m_mediaWidget, &QnMediaResourceWidget::positionChanged
        , this, &WidgetData::updatePos);
}

QnWorkbenchItemBookmarksWatcher::WidgetData::~WidgetData()
{
    disconnect(m_query, nullptr, this, nullptr);
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updatePos(qint64 posMs)
{
    if (m_posMs == posMs)
    {
        sendBookmarksToCompositeOverlay();
        return;
    }

    m_posMs = posMs;

    updateBookmarksAtPosition();
    updateQueryFilter();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateQueryFilter()
{
    const auto newWindow = (m_posMs == DATETIME_INVALID
        ? helpers::extendTimeWindow(m_posMs, m_posMs, kLeftOffset, kRightOffset)
        : QnTimePeriod::fromInterval(DATETIME_INVALID, DATETIME_INVALID));

    auto filter = m_query->filter();
    const bool changed = helpers::isTimeWindowChanged(newWindow.startTimeMs, newWindow.endTimeMs()
        , filter.startTimeMs, filter.endTimeMs, kMinWindowChange);
    if (!changed)
        return;

    filter.startTimeMs = newWindow.startTimeMs;
    filter.endTimeMs = newWindow.endTimeMs();
    m_query->setFilter(filter);
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateBookmarksAtPosition()
{
    QnCameraBookmarkList bookmarks;
    const auto range = std::equal_range(m_bookmarks.begin(), m_bookmarks.end(), m_posMs);
    std::copy(range.first, range.second, std::back_inserter(bookmarks));

    m_bookmarksAtPos.setBookmarkList(bookmarks);
    if (m_timelineWatcher)
        m_bookmarksAtPos.mergeBookmarkList(m_timelineWatcher->bookmarksAtPosition(m_posMs));

    sendBookmarksToCompositeOverlay();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateBookmarks(const QnCameraBookmarkList &newBookmarks)
{
    m_bookmarks = newBookmarks;
    updateBookmarksAtPosition();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::sendBookmarksToCompositeOverlay()
{
    if (!m_mediaWidget)
        return;

    const auto compositeTextOverlay = m_mediaWidget->compositeTextOverlay();
    if (!compositeTextOverlay)
        return;


    compositeTextOverlay->resetModeData(QnCompositeTextOverlay::kBookmarksMode);

    const QnBookmarkColors colors = m_parent->bookmarkColors();
    for (const auto &bookmark: m_bookmarksAtPos.bookmarkList())
    {
        if (bookmark.name.trimmed().isEmpty() && bookmark.description.trimmed().isEmpty())
            continue;

        compositeTextOverlay->addModeData(QnCompositeTextOverlay::kBookmarksMode
            , makeBookmarkItem(bookmark, colors));
    }
}

//

QnWorkbenchItemBookmarksWatcher::QnWorkbenchItemBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_colors()
    , m_widgetDataHash()
{
    connect(context()->display(), &QnWorkbenchDisplay::widgetAdded, this
        , [this](QnResourceWidget *resourceWidget)
    {
        if (!resourceWidget)
            return;

        auto mediaWidget = dynamic_cast<QnMediaResourceWidget *>(resourceWidget);
        if (!mediaWidget)
            return;

        if (m_widgetDataHash.contains(mediaWidget))
            return;

        m_widgetDataHash.insert(mediaWidget, WidgetData::create(mediaWidget, context(), this));
    });

    connect(context()->display(), &QnWorkbenchDisplay::widgetAboutToBeRemoved, this
        , [this](QnResourceWidget *resourceWidget)
    {
        if (!resourceWidget)
            return;

        auto mediaWidget = dynamic_cast<QnMediaResourceWidget *>(resourceWidget);
        if (!mediaWidget)
            return;

        m_widgetDataHash.remove(mediaWidget);
    });
}

QnWorkbenchItemBookmarksWatcher::~QnWorkbenchItemBookmarksWatcher()
{}

QnBookmarkColors QnWorkbenchItemBookmarksWatcher::bookmarkColors() const
{
    return m_colors;
}

void QnWorkbenchItemBookmarksWatcher::setBookmarkColors(const QnBookmarkColors &colors)
{
    m_colors = colors;
}
