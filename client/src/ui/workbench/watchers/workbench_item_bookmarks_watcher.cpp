
#include "workbench_item_bookmarks_watcher.h"

#include <utils/common/string.h>
#include <utils/camera/bookmark_helpers.h>
#include <camera/camera_bookmark_aggregation.h>
#include <camera/current_layout_bookmarks_cache.h>
#include <ui/utils/workbench_item_helper.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/timeline_bookmarks_watcher.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/overlays/composite_text_overlay.h>
#include <ui/graphics/items/overlays/text_overlay_widget.h>
#include <ui/graphics/items/controls/time_slider.h> // TODO: #ynikitenkov remove this dependency

namespace
{
    enum { kMaxBookmarksNearThePosition = 512 };

    enum
    {
        kWindowWidthMs = 30000      // 15 seconds before and after current position
        , kLeftOffset = kWindowWidthMs / 2
        , kRightOffset = kLeftOffset

        , kMinWindowChange = kRightOffset / 3
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

class QnWorkbenchItemBookmarksWatcher::ListenerHolderPrivate
{
public:
    ListenerHolderPrivate(const Listeners::iterator it
        , QnWorkbenchItemBookmarksWatcher *watcher)
        : m_it(it)
        , m_watcher(watcher)
    {}


    ~ListenerHolderPrivate()
    {
        m_watcher->removeListener(m_it);
    }

private:
    const Listeners::iterator m_it;
    QnWorkbenchItemBookmarksWatcher * const m_watcher;
};

//

struct QnWorkbenchItemBookmarksWatcher::ListenerDataPrivate
{
    Listener listenerCallback;
    QnCameraBookmarkList bookmarks;

    explicit ListenerDataPrivate(const Listener &listenerCallback)
        : listenerCallback(listenerCallback)
        , bookmarks()
    {}
};

//

QnWorkbenchItemBookmarksWatcher::QnWorkbenchItemBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_aggregationHelper(new QnCameraBookmarkAggregation())
    , m_timelineWatcher(context()->instance<QnTimelineBookmarksWatcher>())
    , m_bookmarksCache(new QnCurrentLayoutBookmarksCache(kMaxBookmarksNearThePosition
        , Qn::EarliestFirst, kMinWindowChange, parent))
    , m_itemListinersMap()
    , m_listeners()
    , m_posMs(0)

    , m_colors()
{
    // TODO: #ynikitenkov remove these dependencies.
    connect(navigator()->timeSlider(), &QnTimeSlider::valueChanged
        , this, &QnWorkbenchItemBookmarksWatcher::updatePosition);

    // TODO: add correct handling of QnCameraBookmarksManager::
    // bookmarkAdded / bookmarkUpdated / bookmarkRemoved
    // For now, there is no practical need - bookmarks are updated every
    // ~5 seconds (by query), but if we decide to extend this period (for
    // 1 minute, for example), we will have to support it
    connect(m_bookmarksCache, &QnCurrentLayoutBookmarksCache::bookmarksChanged
        , this, &QnWorkbenchItemBookmarksWatcher::onBookmarksChanged);
    connect(m_bookmarksCache, &QnCurrentLayoutBookmarksCache::itemAdded
        , this, &QnWorkbenchItemBookmarksWatcher::onItemAdded);
    connect(m_bookmarksCache, &QnCurrentLayoutBookmarksCache::itemRemoved
        , this, &QnWorkbenchItemBookmarksWatcher::onItemRemoved);
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

void QnWorkbenchItemBookmarksWatcher::onItemAdded(QnWorkbenchItem *item)
{
    if (!display())
        return;

    const auto camera = helpers::extractCameraResource(item);
    if (!camera)
        return;

    auto resource = display()->widget(item->uuid());
    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget*>(resource);
    if (!widget)
        return;

    const auto bookmarksAtPosUpdatedCallback = [this, widget](const QnCameraBookmarkList &bookmarks)
    {
        auto compositeTextOverlay = widget->compositeTextOverlay();
        if (!compositeTextOverlay)
            return;

        compositeTextOverlay->resetModeData(QnCompositeTextOverlay::kBookmarksMode);
        for (auto bookmark: bookmarks)
        {
            if (bookmark.name.trimmed().isEmpty() && bookmark.description.trimmed().isEmpty())
                continue;

            compositeTextOverlay->addModeData(QnCompositeTextOverlay::kBookmarksMode
                , makeBookmarkItem(bookmark, m_colors));
        }
    };

    const auto listenerHolder = addListener(camera, bookmarksAtPosUpdatedCallback);
    m_itemListinersMap.insert(item, listenerHolder);
}

void QnWorkbenchItemBookmarksWatcher::onItemRemoved(QnWorkbenchItem *item)
{
    const auto camera = helpers::extractCameraResource(item);
    if (!camera)
        return;

    m_itemListinersMap.remove(item);
}

void QnWorkbenchItemBookmarksWatcher::updatePosition(qint64 posMs)
{
    m_posMs = posMs;
    const QnTimePeriod newWindow = QnTimePeriod::createFromInterval(
        posMs - kLeftOffset, posMs + kRightOffset);

    m_bookmarksCache->setWindow(newWindow);

    for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
        const auto camera = it.key();
        updateListenerData(it, m_bookmarksCache->bookmarks(camera));
    }
}

void QnWorkbenchItemBookmarksWatcher::onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkList &bookmarks)
{
    const auto range = m_listeners.equal_range(camera);
    for (auto it = range.first; it != range.second; ++it)
        updateListenerData(it, bookmarks);
}

QnWorkbenchItemBookmarksWatcher::ListenerLifeHolder QnWorkbenchItemBookmarksWatcher::addListener(
    const QnVirtualCameraResourcePtr &camera
    , const Listener &listenerCallback)
{
    const auto iterator = m_listeners.insertMulti(camera
        , ListenerDataPrivatePtr(new ListenerDataPrivate(listenerCallback)));
    return ListenerLifeHolder(new ListenerHolderPrivate(iterator, this));
}

void QnWorkbenchItemBookmarksWatcher::removeListener(const Listeners::iterator &it)
{
    m_listeners.erase(it);
}

void QnWorkbenchItemBookmarksWatcher::updateListenerData(const Listeners::iterator &it
    , const QnCameraBookmarkList &bookmarks)
{
    const auto camera = it.key();
    auto &listenerData = it.value();

    // We have to source for bookmarks:
    //  *   Current watcher bookmarks for item - constrained by small period near the current position.
    //      If bookmarks query has completed we have all bookmarks for current position
    //  *   Bookmarks at timeline watcher - there could be not all bookmarks for position,
    //      but they could exist before current watcher load bookmarks for current position.
    // Thus, we could merge them and get best result by load speed (from one side) and bookmarks presence
    // for specified position

    m_aggregationHelper->setBookmarkList(helpers::bookmarksAtPosition(bookmarks, m_posMs));
    m_aggregationHelper->mergeBookmarkList(m_timelineWatcher->rawBookmarksAtPosition(camera, m_posMs));

    const auto currentBookmarksList = m_aggregationHelper->bookmarkList();

    if (listenerData->bookmarks == currentBookmarksList)
        return;

    listenerData->bookmarks = currentBookmarksList;
    listenerData->listenerCallback(currentBookmarksList);
}

