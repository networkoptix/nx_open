// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_item_bookmarks_watcher.h"

#include <chrono>

#include <camera/camera_bookmark_aggregation.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/datetime.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/bookmark/bookmark_helpers.h>
#include <nx/vms/common/html/html.h>
#include <ui/graphics/items/overlays/scrollable_text_items_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/utils/workbench_item_helper.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_timer.h>
#include <utils/common/synctime.h>

using namespace std::chrono;
using namespace nx::vms::client::core;
using namespace nx::vms::client::desktop;

namespace
{
    // 5 minutes before and after current position
    static constexpr auto kWindowWidthMs =  5 * 60 * 1000;

    static constexpr auto kLeftOffset = kWindowWidthMs / 2;
    static constexpr auto kRightOffset = kLeftOffset;

    static constexpr auto kMaxBookmarksNearThePosition = 256;

    const QnCameraBookmarkSearchFilter kInitialFilter{.limit=kMaxBookmarksNearThePosition};

    /* Maximum number of bookmarks, allowed on the single widget. */
    const int kBookmarksDisplayLimit = 10;

    /* Ignore position changes less than given time period. */
    const qint64 kMinPositionChangeMs = 100;

    /* Periods to update bookmarks query. */
    const milliseconds kMinWindowChangeNearLiveMs = 10s;
    const milliseconds kMinWindowChangeInArchiveMs = 2min;

    struct QnOverlayTextItemData
    {
        QnUuid id;
        QString text;
        QnHtmlTextItemOptions options;
    };

    QnOverlayTextItemData makeBookmarkItem(const QnCameraBookmark& bookmark)
    {
        using namespace nx::vms::common;

        //static const QColor kBackgroundColor("#b22e6996");
        static const QColor kBackgroundColor = nx::vms::client::core::ColorTheme::transparent(
            nx::vms::client::core::colorTheme()->color("camera.bookmarkBackground"), 0.7);

        enum
        {
            kBorderRadius = 4
            , kPadding = 8

            , kCaptionMaxLength = 64
            , kDescriptionMaxLength = 160
            , kMaxItemWidth = 250
        };

        static QnHtmlTextItemOptions options = QnHtmlTextItemOptions(
            kBackgroundColor, false, kBorderRadius, kPadding, kPadding, kMaxItemWidth);

        enum
        {
            kCaptionPixelSize = 16
            , kDescriptionPixeSize = 12
        };

        const auto captionHtml = html::elide(
            html::styledParagraph(
                bookmark.name.toHtmlEscaped(),
                kCaptionPixelSize,
                /*bold*/true),
            kCaptionMaxLength);
        const auto descriptionHtml = html::elide(
            html::styledParagraph(
                html::toHtml(bookmark.description),
                kDescriptionPixeSize),
            kDescriptionMaxLength);

        static const QString kHtmlPageTemplate = "<html><head><style>*"
            " {text-indent: 0; margin-top: 0; margin-bottom: 0; margin-left: 0;"
            " margin-right: 0; color: white;}</style></head><body>%1</body></html>";

        const auto bookmarkHtml = kHtmlPageTemplate.arg(captionHtml + descriptionHtml);
        return QnOverlayTextItemData({ bookmark.guid, bookmarkHtml, options });
    };
}

//

class QnWorkbenchItemBookmarksWatcher::WidgetData : public QObject
{
    typedef QObject base_type;

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

    void sendBookmarksToOverlay();

private:
    void updatePos(qint64 posMs);

    void updateQueryFilter();

    void updateBookmarks(const QnCameraBookmarkList &newBookmarks);

    void updateBookmarksAtPosition();

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

QnWorkbenchItemBookmarksWatcher::WidgetData::WidgetData(
    const QnVirtualCameraResourcePtr& camera,
    QnMediaResourceWidget* resourceWidget,
    QnTimelineBookmarksWatcher* timelineWatcher,
    QnWorkbenchItemBookmarksWatcher* parent)
    :
    base_type(parent),
    m_timelineWatcher(timelineWatcher),
    m_camera(camera),
    m_parent(parent),
    m_mediaWidget(resourceWidget),
    m_posMs(DATETIME_INVALID),
    m_bookmarks(),
    m_bookmarksAtPos(),
    m_query(nx::vms::client::desktop::SystemContext::fromResource(camera)->
        cameraBookmarksManager()->createQuery())
{
    m_query->setFilter(kInitialFilter);
    m_query->setCamera(camera->getId());
    connect(m_query.get(), &QnCameraBookmarksQuery::bookmarksChanged,
        this, &WidgetData::updateBookmarks);

    connect(m_mediaWidget, &QnMediaResourceWidget::positionChanged,
        this, &WidgetData::updatePos);

    m_query->setActive(m_parent->navigator()->bookmarksModeEnabled());
    connect(m_parent->navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged, this,
        [this]()
        {
            const auto bookmarksModeEnabled = m_parent->navigator()->bookmarksModeEnabled();
            m_query->setActive(bookmarksModeEnabled);
        });
}

QnWorkbenchItemBookmarksWatcher::WidgetData::~WidgetData()
{
    disconnect(m_query.get(), nullptr, this, nullptr);
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
    // No need to update filter if current position is invalid (e.g. client was disconnected or
    // camera was removed).
    if (m_posMs == DATETIME_INVALID)
        return;

    using namespace nx::vms::common;
    const auto newWindow = extendTimeWindow(m_posMs, m_posMs, kLeftOffset, kRightOffset);

    bool nearLive = m_posMs + kRightOffset >= qnSyncTime->currentMSecsSinceEpoch();
    const auto minWindowChange = nearLive
        ? kMinWindowChangeNearLiveMs
        : kMinWindowChangeInArchiveMs;

    auto filter = m_query->filter();
    const bool changed = isTimeWindowChanged(
        milliseconds(newWindow.startTimeMs),
        milliseconds(newWindow.endTimeMs()),
        filter.startTimeMs,
        filter.endTimeMs,
        minWindowChange);
    if (!changed)
        return;

    filter.startTimeMs = milliseconds(newWindow.startTimeMs);
    filter.endTimeMs = milliseconds(newWindow.endTimeMs());
    m_query->setFilter(filter);
}

void QnWorkbenchItemBookmarksWatcher::WidgetData::updateBookmarksAtPosition()
{
    QnCameraBookmarkList bookmarks;

    const auto endTimeGreaterThanPos = [this](const QnCameraBookmark &bookmark)
    { return (bookmark.endTime().count() > m_posMs); };

    const auto itEnd = std::upper_bound(m_bookmarks.begin(), m_bookmarks.end(), milliseconds(m_posMs));
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
    const auto bookmarksContainer = m_mediaWidget->bookmarksContainer();
    if (!bookmarksContainer)
        return;

    QnCameraBookmarkList bookmarksToDisplay;
    QnCameraBookmarkSearchFilter filter;
    filter.text = m_parent->m_textFilter;
    filter.endTimeMs = milliseconds(DATETIME_NOW);

    for (const auto& bookmark: m_bookmarksAtPos.bookmarkList())
    {
        if (bookmark.name.trimmed().isEmpty() && bookmark.description.trimmed().isEmpty())
            continue;

        if (!filter.checkBookmark(bookmark))
            continue;

        bookmarksToDisplay.push_back(bookmark);
        if (bookmarksToDisplay.size() >= kBookmarksDisplayLimit)
            break;
    }

    if (m_displayedBookmarks == bookmarksToDisplay)
        return;

    m_displayedBookmarks = bookmarksToDisplay;

    bookmarksContainer->clear();

    for (const auto& bookmark: bookmarksToDisplay)
    {
        const auto item = makeBookmarkItem(bookmark);
        bookmarksContainer->addItem(item.text, item.options, item.id);
    }
}

//

QnWorkbenchItemBookmarksWatcher::QnWorkbenchItemBookmarksWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
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

        WidgetDataPtr data = WidgetData::create(mediaWidget, context(), this);
        if (data) //< Data is not created for local files.
            m_widgetDataHash.insert(mediaWidget, data);
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

void QnWorkbenchItemBookmarksWatcher::setDisplayFilter(
    const QnVirtualCameraResourceSet& cameraFilter, const QString& textFilter)
{
    if (m_textFilter == textFilter && m_cameraFilter == cameraFilter)
        return;

    m_cameraFilter = cameraFilter;
    m_textFilter = textFilter;

    for (auto& widgetData: m_widgetDataHash)
        widgetData->sendBookmarksToOverlay();
}
