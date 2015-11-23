
#include "composite_text_overlay.h"

#include <api/common_message_processor.h>

#include <utils/common/string.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include <business/business_strings_helper.h>
#include <business/actions/abstract_business_action.h>

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>

#include <ui/workbench/workbench_navigator.h>
#include <ui/common/search_query_strategy.h>
#include <ui/common/search_query_strategy.h>

namespace
{
    enum 
    {
        kCaptionMaxLength = 128
        , kDescriptionMaxLength = 256
    };

    QString makePTag(const QString &text
        , int pixelSize
        , bool isBold = false
        , bool isItalic = false)
    {
        static const auto kPTag = lit("<p style=\" text-ident: 0; font-size: %2px; font-weight: %3; font-style: %4; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; \">%1</p>");

        if (text.isEmpty())
            return QString();

        const QString boldValue = (isBold ? lit("bold") : lit("normal"));
        const QString italicValue (isItalic ? lit("italic") : lit("normal"));
        return kPTag.arg(text, QString::number(pixelSize), boldValue, italicValue);
    }

    enum { bookmarksFilterPrecisionMs = 5 * 60 * 1000 };

    QnCameraBookmarkSearchFilter constructBookmarksFilter(qint64 positionMs, const QString &text = QString()) {
        if (positionMs <= 0)
            return QnCameraBookmarkSearchFilter::invalidFilter();

        QnCameraBookmarkSearchFilter result;

        /* Round the current time to reload bookmarks only once a time. */
        qint64 mid = positionMs - (positionMs % bookmarksFilterPrecisionMs);

        result.startTimeMs = mid - bookmarksFilterPrecisionMs;

        /* Seek forward twice as long so when the mid point changes, next period will be preloaded. */
        result.endTimeMs = mid + bookmarksFilterPrecisionMs * 2;

        result.text = text;

        return result;
    }

    //TODO: #GDM #Bookmarks #duplicate code
    QnCameraBookmarkList getBookmarksAtPosition(const QnCameraBookmarkList &bookmarks, qint64 position) {
        QnCameraBookmarkList result;
        std::copy_if(bookmarks.cbegin(), bookmarks.cend(), std::back_inserter(result), [position](const QnCameraBookmark &bookmark) {
            return bookmark.startTimeMs <= position && position < bookmark.endTimeMs();
        });
        return result;
    }

}

QnCompositeTextOverlay::QnCompositeTextOverlay(const QnVirtualCameraResourcePtr &camera
    , QnWorkbenchNavigator *navigator
    , const GetUtcCurrentTimeMsFunc &getCurrentUtcTimeMs
    , QGraphicsWidget *parent)

    : base_type(parent)

    , m_camera(camera)
    , m_getUtcCurrentTimeMs(getCurrentUtcTimeMs)

    , m_counter()
    , m_navigator(navigator)
    
    , m_currentMode(kUndefinedMode)
    , m_data()

    , m_updateQueryFilterTimer()
    , m_updateBookmarksTimer()

    , m_colors()
{
    m_counter.start();
    initTextMode();
    initBookmarksMode();
}

QnCompositeTextOverlay::~QnCompositeTextOverlay()
{
}

void QnCompositeTextOverlay::setMode(Mode mode)
{
    if (mode == m_currentMode)
        return;

    const Mode prevMode = m_currentMode;
    m_currentMode = mode;

    currentModeChanged(prevMode, m_currentMode);

    setItemsData(filterModeData(m_currentMode));
}

void QnCompositeTextOverlay::addModeData(Mode mode
    , const QnOverlayTextItemData &data)
{
    auto &currentData = m_data[mode];
    const auto it = findModeData(mode, data.id);
    const auto innerData = InnerData(m_counter.elapsed(), data);
    if (it == currentData.end())
        currentData.append(innerData);
    else
        *it = innerData;

    if (mode == m_currentMode)
        addItemData(data);
}

void QnCompositeTextOverlay::removeModeData(Mode mode
    , const QnUuid &id)
{
    auto &currentData = m_data[mode];
    const auto it = findModeData(mode, id);
    if (it == currentData.end())
        return;

    currentData.erase(it);

    if (m_currentMode == mode)
        removeItemData(id);
}

void QnCompositeTextOverlay::setModeData(Mode mode
    , const InnerDataContainer &data)
{

    auto &currentData = m_data[mode];
    if (currentData == data)
        return;

    m_data[mode] = data;

    if (m_currentMode == mode)
        setItemsData(filterModeData(m_currentMode));
}

void QnCompositeTextOverlay::resetModeData(Mode mode)
{
    setModeData(mode, InnerDataContainer());
}

QnCompositeTextOverlay::InnerDataContainer::Iterator QnCompositeTextOverlay::findModeData(
    Mode mode
    , const QnUuid &id)
{
    auto &currentData = m_data[mode];
    const auto it = std::find_if(currentData.begin(), currentData.end()
        , [id](const InnerData &data)
    {
        return (data.second.id == id);
    });
    return it;
}

void QnCompositeTextOverlay::initTextMode()
{
    if (!m_camera)
        return;

    const auto cameraId = m_camera->getId();
    const auto messageProcessor = QnCommonMessageProcessor::instance();

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived
        , this, [this, cameraId](const QnAbstractBusinessActionPtr &businessAction)
    {
        if (businessAction->actionType() != QnBusiness::ShowTextOverlayAction)
            return;

        const auto &actionParams = businessAction->getParams();
        if (!businessAction->getResources().contains(cameraId))
            return;

        const auto state = businessAction->getToggleState();
        const bool isInstantAction = (state == QnBusiness::UndefinedState);
        const auto actionId = (isInstantAction ? QnUuid::createUuid() : businessAction->getBusinessRuleId());

        if (state == QnBusiness::InactiveState)
        {
            removeModeData(QnCompositeTextOverlay::kTextAlaramsMode, actionId);

            return;
        }

        enum { kDefaultInstantActionTimeout = 5000 };
        const int timeout = (isInstantAction
            ? (actionParams.durationMs ? actionParams.durationMs : kDefaultInstantActionTimeout)
            : QnOverlayTextItemData::kNoTimeout);

        QString text;
        if (actionParams.text.isEmpty())
        {
            const auto runtimeParams = businessAction->getRuntimeParams();
            const auto caption = elideString(
                QnBusinessStringsHelper::eventAtResource(runtimeParams, true), kCaptionMaxLength);

            const auto desciption = elideString(
                QnBusinessStringsHelper::eventDetails(runtimeParams, lit("\n")), kDescriptionMaxLength);

            static const auto kComplexHtml = lit("<html><body>%1%2</body></html>");
            text = kComplexHtml.arg(makePTag(caption, 16, true)
                , makePTag(desciption, 13));
        }
        else
        {
            static const auto kTextHtml = lit("<html><body>%1</body></html>");
            text = elideString(kTextHtml.arg(actionParams.text), kDescriptionMaxLength);
        }

        const QnHtmlTextItemOptions options(m_colors.textOverlayItemColor, true, 2, 12, 250);

        const QnOverlayTextItemData data(actionId, text, options, timeout);
        addModeData(QnCompositeTextOverlay::kTextAlaramsMode, data);
    });
}

void QnCompositeTextOverlay::initBookmarksMode()
{
    //// TODO: #ynikitenkov Refactor this according to logic in QnWorkbenchNavigator (use cache of bookmark queries)

    connect(m_navigator, &QnWorkbenchNavigator::positionChanged, this, &QnCompositeTextOverlay::updateBookmarksFilter);
    connect(m_navigator->bookmarksSearchStrategy(), &QnSearchQueryStrategy::queryUpdated
        , this, &QnCompositeTextOverlay::updateBookmarksFilter);

    /* Update bookmarks by timer to preload new bookmarks smoothly when playing archive. */
    m_updateQueryFilterTimer.reset([this]()
    {
        QTimer *timer = new QTimer();
        timer->setInterval(bookmarksFilterPrecisionMs / 2);
        connect(timer, &QTimer::timeout, this, &QnCompositeTextOverlay::updateBookmarksFilter);
        timer->start();
        return timer;
    }());

    /* Update bookmarks text by timer. */
    m_updateBookmarksTimer.reset([this]()
    {
        enum { kUpdateBookmarksPeriod = 1000 };
        
        QTimer* timer = new QTimer(this);
        timer->setInterval(kUpdateBookmarksPeriod);
        connect(timer, &QTimer::timeout, this, &QnCompositeTextOverlay::updateBookmarks);
        timer->start();
        return timer;
    }());
}

QnMediaResourceWidgetColors QnCompositeTextOverlay::colors() const
{
    return m_colors;
}

void QnCompositeTextOverlay::setColors(const QnMediaResourceWidgetColors &colors)
{
    m_colors = colors;

    // TODO: #ynikitenkov Update colors on current items
    const auto tmp = m_data[m_currentMode];
    resetModeData(m_currentMode);
    setModeData(m_currentMode, tmp);
}

void QnCompositeTextOverlay::currentModeChanged(Mode prev
    , Mode current)
{
    if ((prev != kBookmarksMode) && (current != kBookmarksMode))
        return;

    const bool bookmarksEnabled = (current == kBookmarksMode);
    if (m_bookmarksQuery.isNull() != bookmarksEnabled)
        return;

    if (bookmarksEnabled) 
    {
        m_bookmarksQuery = qnCameraBookmarksManager->createQuery();

        connect(m_bookmarksQuery, &QnCameraBookmarksQuery::bookmarksChanged, this
            , &QnCompositeTextOverlay::updateBookmarks);
        updateBookmarksFilter();
        m_bookmarksQuery->setCamera(m_camera);
    } 
    else 
    {
        if (m_bookmarksQuery)
            disconnect(m_bookmarksQuery, nullptr, this, nullptr);

        m_bookmarksQuery.clear();
    }
    updateBookmarks();
}

void QnCompositeTextOverlay::updateBookmarks()
{
    if (!m_bookmarksQuery) 
    {
        resetModeData(QnCompositeTextOverlay::kBookmarksMode);
        return;
    }

    if (!m_bookmarksQuery->filter().isValid())
        updateBookmarksFilter();

    const auto cached = m_bookmarksQuery->cachedBookmarks();
    const auto bookmarks = getBookmarksAtPosition(cached, m_getUtcCurrentTimeMs());

    setModeData(QnCompositeTextOverlay::kBookmarksMode
        , makeTextItemData(bookmarks, m_colors.bookmarkColors));
}

void QnCompositeTextOverlay::updateBookmarksFilter()
{
    if (!m_bookmarksQuery)
        return;

    m_bookmarksQuery->setFilter(constructBookmarksFilter(m_getUtcCurrentTimeMs()
        , m_navigator->bookmarksSearchStrategy()->query()));
}

QnOverlayTextItemDataList QnCompositeTextOverlay::filterModeData(Mode mode)
{
    const auto currentTimestamp = m_counter.elapsed();

    auto &data = m_data[mode];
    const auto lastIt = std::remove_if(data.begin(), data.end()
        , [this, currentTimestamp](const InnerData &innerData)
    {
        const auto timeout = innerData.second.timeout;
        const bool timeoutExpired = ((timeout > 0) 
            && ((currentTimestamp - innerData.first) > timeout));

        return timeoutExpired;
    });

    data.erase(lastIt, data.end());


    QnOverlayTextItemDataList result;
    for (auto &innerData: data)
    {
        auto &itemTimeout = innerData.second.timeout;
        if (itemTimeout > 0)
            itemTimeout -= (currentTimestamp - innerData.first);
        result.append(innerData.second);
    }
    return result;
}

QnCompositeTextOverlay::InnerDataContainer QnCompositeTextOverlay::makeTextItemData(
    const QnCameraBookmarkList &bookmarks
    , const QnBookmarkColors &colors)
{
    static QnHtmlTextItemOptions options = QnHtmlTextItemOptions(
        colors.background, false, 4, 8, 250);

    static const auto kBookTemplate = lit("<html><body>%1%2</body></html>");

    InnerDataContainer data;

    const auto timestamp = m_counter.elapsed();
    for (const auto bookmark: bookmarks)
    {
        const auto bookmarkHtml = kBookTemplate.arg(
            makePTag(bookmark.name, 16, true)
            , makePTag(bookmark.description, 12));

        const auto itemData = QnOverlayTextItemData(bookmark.guid, bookmarkHtml, options);
        data.push_back(InnerData(timestamp, itemData));
    }
    return data;
};
