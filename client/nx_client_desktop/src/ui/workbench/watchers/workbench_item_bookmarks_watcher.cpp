
#include "workbench_item_bookmarks_watcher.h"

#include <utils/common/html.h>

#include <utils/camera/bookmark_helpers.h>
#include <camera/camera_bookmark_aggregation.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>
#include <ui/utils/workbench_item_helper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <utils/common/scoped_timer.h>
#include <utils/common/synctime.h>

#include <nx/utils/datetime.h>

namespace
{
    enum
    {
        kWindowWidthMs =  5 * 60 * 1000      // 3 minute before and after current position
        , kLeftOffset = kWindowWidthMs / 2
        , kRightOffset = kLeftOffset
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

    /* Maximum number of bookmarks, allowed on the single widget. */
    const int kBookmarksDisplayLimit = 10;

    /* Ignore position changes less than given time period. */
    const qint64 kMinPositionChangeMs = 100;

    /* Periods to update bookmarks query. */
    const qint64 kMinWindowChangeNearLiveMs = 10000;
    const qint64 kMinWindowChangeInArchiveMs = 2 * 60 * 1000;

    struct QnOverlayTextItemData
    {
        QnUuid id;
        QString text;
        QnHtmlTextItemOptions options;
    };

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
            bookmark.name.toHtmlEscaped(), kCaptionPixelSize, true), kCaptionMaxLength);
        const auto descHtml = elideHtml(htmlFormattedParagraph(
            ensureHtml(bookmark.description), kDescriptionPixeSize), kDescriptionMaxLength);

        static const auto kHtmlPageTemplate = lit("<html><head><style>* {text-ident: 0; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; color: white;}</style></head><body>%1</body></html>");
        static const auto kComplexHtml = lit("%1%2");

        const auto bookmarkHtml = kHtmlPageTemplate.arg(kComplexHtml.arg(captionHtml, descHtml));
        return QnOverlayTextItemData({ bookmark.guid, bookmarkHtml, options });
    };
}

//

class QnWorkbenchItemBookmarksWatcher::WidgetData : public Connective<QObject>
{
    typedef Connective<QObject> base_type;

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

    void sendBookmarksToOverlay();

private:
    const QnTimelineBookmarksWatcherPtr m_timelineWatcher;
    const QnVirtualCameraResourcePtr m_camera;
    QnWorkbenchItemBookmarksWatcher * const m_parent;
    QnMediaResourceWidget * const m_mediaWidget;

    qint64 m_posMs;
    QnCameraBookmarkList m_bookmarks;
    QnCameraBookmarkAggregation m_bookmarksAtPos;
    QnCameraBookmarkList m_displayedBookmarks;
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
    , m_timelineWatcher(timelineWatcher)
    , m_camera(camera)
    , m_parent(parent)
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
        return; /* Really should never get here. */

    const bool updateRequired = (m_posMs == DATETIME_INVALID)
        || (qAbs(m_posMs - posMs) >= kMinPositionChangeMs);

    if (!updateRequired)
        return;

    m_posMs = posMs;

    updateBookmarksAtPosition();
    updateQueryFilter();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateQueryFilter()
{
    QN_LOG_TIME(Q_FUNC_INFO);

    const auto newWindow = (m_posMs == DATETIME_INVALID
        ? QnTimePeriod::fromInterval(DATETIME_INVALID, DATETIME_INVALID)
        : helpers::extendTimeWindow(m_posMs, m_posMs, kLeftOffset, kRightOffset));

    bool nearLive = m_posMs + kRightOffset >= qnSyncTime->currentMSecsSinceEpoch();
    const qint64 minWindowChange = nearLive
        ? kMinWindowChangeNearLiveMs
        : kMinWindowChangeInArchiveMs;

    auto filter = m_query->filter();
    const bool changed = helpers::isTimeWindowChanged(newWindow.startTimeMs, newWindow.endTimeMs()
        , filter.startTimeMs, filter.endTimeMs, minWindowChange);
    if (!changed)
        return;

    filter.startTimeMs = newWindow.startTimeMs;
    filter.endTimeMs = newWindow.endTimeMs();
    m_query->setFilter(filter);
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateBookmarksAtPosition()
{
    QN_LOG_TIME(Q_FUNC_INFO);

    QnCameraBookmarkList bookmarks;

    const auto endTimeGreaterThanPos = [this](const QnCameraBookmark &bookmark)
    { return (bookmark.endTimeMs() > m_posMs); };

    const auto itEnd = std::upper_bound(m_bookmarks.begin(), m_bookmarks.end(), m_posMs);
    std::copy_if(m_bookmarks.begin(), itEnd, std::back_inserter(bookmarks), endTimeGreaterThanPos);

    m_bookmarksAtPos.setBookmarkList(bookmarks);
    if (m_timelineWatcher)
        m_bookmarksAtPos.mergeBookmarkList(m_timelineWatcher->rawBookmarksAtPosition(m_camera, m_posMs));

    sendBookmarksToOverlay();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateBookmarks(const QnCameraBookmarkList &newBookmarks)
{
    if (m_bookmarks == newBookmarks)
        return;

    m_bookmarks = newBookmarks;
    updateBookmarksAtPosition();
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::sendBookmarksToOverlay()
{
    QN_LOG_TIME(Q_FUNC_INFO);

    const auto bookmarksContainer = m_mediaWidget->bookmarksContainer();
    if (!bookmarksContainer)
        return;

    QnCameraBookmarkList bookmarksToDisplay;
    for (const auto &bookmark: m_bookmarksAtPos.bookmarkList())
    {
        if (bookmark.name.trimmed().isEmpty() && bookmark.description.trimmed().isEmpty())
            continue;

        bookmarksToDisplay << bookmark;
        if (bookmarksToDisplay.size() >= kBookmarksDisplayLimit)
            break;
    }

    if (m_displayedBookmarks == bookmarksToDisplay)
        return;

    m_displayedBookmarks = bookmarksToDisplay;

    bookmarksContainer->clear();

    const QnBookmarkColors colors = m_parent->bookmarkColors();
    for (const auto& bookmark: bookmarksToDisplay)
    {
        const auto item = makeBookmarkItem(bookmark, colors);
        bookmarksContainer->addItem(item.text, item.options, item.id);
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
