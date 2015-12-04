
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
        kCaptionMaxLength = 64
        , kDescriptionMaxLength = 96
        , kMaxItemWidth = 250
    };



    enum { kBookmarksFilterPrecisionMs = 5 * 60 * 1000 };

    QnCameraBookmarkSearchFilter constructBookmarksFilter(qint64 positionMs) {
        if (positionMs <= 0)
            return QnCameraBookmarkSearchFilter::invalidFilter();

        QnCameraBookmarkSearchFilter result;

        /* Round the current time to reload bookmarks only once a time. */
        qint64 mid = positionMs - (positionMs % kBookmarksFilterPrecisionMs);

        result.startTimeMs = mid - kBookmarksFilterPrecisionMs;

        /* Seek forward twice as long so when the mid point changes, next period will be preloaded. */
        result.endTimeMs = mid + kBookmarksFilterPrecisionMs * 2;

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

    connect(this, &QnCompositeTextOverlay::modeChanged
        , this, &QnCompositeTextOverlay::at_modeChanged);
}

QnCompositeTextOverlay::~QnCompositeTextOverlay()
{
}

QnCompositeTextOverlay::Mode QnCompositeTextOverlay::mode() const
{
    return m_currentMode;
}

void QnCompositeTextOverlay::setMode(Mode mode)
{
    if (mode == m_currentMode)
        return;

    m_currentMode = mode;

    setTextItems(removeOutdatedItems(m_currentMode));

    emit modeChanged();
}

void QnCompositeTextOverlay::addModeData(Mode mode
    , const QnOverlayTextItemData &data)
{
    auto &currentData = m_data[mode];
    const auto it = findModeData(mode, data.id);
    const auto internalData = InternalData(m_counter.elapsed(), data);
    if (it == currentData.end())
        currentData.insert(data.id, internalData);
    else
        *it = internalData;

    if (mode == m_currentMode)
        addTextItem(data);
}

void QnCompositeTextOverlay::removeModeData(Mode mode
    , const QnUuid &id)
{
    auto &currentData = m_data[mode];
    auto it = findModeData(mode, id);
    if (it == currentData.end())
        return;

    enum { kMinDataLifetimeMs = 5000 };

    // Do not remove data too fast (for short prolonged actions, for instance)
    const auto dataTimestamp = it->first;
    const auto currentLifetime = (m_counter.elapsed() - dataTimestamp);
    if (currentLifetime < kMinDataLifetimeMs)
    {
        auto &dataToBeUpdated = it->second;
        if (m_currentMode == mode)
        {
            // Sets timeout to remove item automatically
            dataToBeUpdated.timeout = (kMinDataLifetimeMs - currentLifetime);
            addTextItem(dataToBeUpdated);   // replaces existing data
        }
        else
        {
            // do not change timestamp, but set timeout to be sure
            // we delete item on next mode switch
            dataToBeUpdated.timeout = kMinDataLifetimeMs;
        }

        return;
    }

    currentData.erase(it);

    if (m_currentMode == mode)
        removeTextItem(id);
}

void QnCompositeTextOverlay::setModeData(Mode mode
    , const InternalDataHash &data)
{

    auto &currentData = m_data[mode];
    if (currentData == data)
        return;

    m_data[mode] = data;

    if (m_currentMode == mode)
        setTextItems(removeOutdatedItems(m_currentMode));
}

void QnCompositeTextOverlay::resetModeData(Mode mode)
{
    setModeData(mode, InternalDataHash());
}

QnCompositeTextOverlay::InternalDataHash::Iterator QnCompositeTextOverlay::findModeData(Mode mode
    , const QnUuid &id)
{
    auto &currentData = m_data[mode];
    return currentData.find(id);
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
        if (actionParams.actionResourceId != cameraId)
            return;

        enum { kDefaultInstantActionTimeoutMs = 5000 };

        const auto state = businessAction->getToggleState();
        const bool isInstantAction = (actionParams.durationMs > 0) || (state == QnBusiness::UndefinedState);
        const int timeout = (isInstantAction
            ? (actionParams.durationMs > 0 ? actionParams.durationMs : kDefaultInstantActionTimeoutMs)
            : QnOverlayTextItemData::kInfinite);

        const auto actionId = businessAction->getBusinessRuleId();

        if (!isInstantAction && (state == QnBusiness::InactiveState))
        {
            removeModeData(QnCompositeTextOverlay::kTextOutputMode, actionId);

            return;
        }

        enum
        {
            kDescriptionPixelFontSize = 13
            , kCaptionPixelFontSize = 16
            , kHorPaddings = 12
            , kVertPaddings = 8
            , kBorderRadius = 2
        };

        QString text;
        if (actionParams.text.isEmpty())
        {
            const auto runtimeParams = businessAction->getRuntimeParams();
            const auto caption = elideString(
                QnBusinessStringsHelper::eventAtResource(runtimeParams, true), kCaptionMaxLength);

            const auto desciption = elideString(
                QnBusinessStringsHelper::eventDetails(runtimeParams, lit("\n")), kDescriptionMaxLength);

            if (caption.trimmed().isEmpty() && desciption.trimmed().isEmpty())  // Do not add empty text items
                return;

            static const auto kComplexHtml = lit("%1%2");
            text = kComplexHtml.arg(htmlFormattedParagraph(caption, kCaptionPixelFontSize, true)
                , htmlFormattedParagraph(desciption, kDescriptionPixelFontSize));
        }
        else
        {
            if (actionParams.text.trimmed().isEmpty()) // Do not add empty text items
                return;
            static const auto kTextHtml = lit("<html><body>%1</body></html>");
            const auto elided = elideString(actionParams.text, kDescriptionMaxLength);
            text = kTextHtml.arg(htmlFormattedParagraph(elided, 13));
        }

        const QnHtmlTextItemOptions options(m_colors.textOverlayItemColor, true
            , kBorderRadius, kHorPaddings, kVertPaddings, kMaxItemWidth);

        const QnOverlayTextItemData data(actionId, text, options, timeout);
        addModeData(QnCompositeTextOverlay::kTextOutputMode, data);
    });
}

void QnCompositeTextOverlay::initBookmarksMode()
{
    // TODO: #ynikitenkov Refactor this according to logic in QnWorkbenchNavigator (use cache of bookmark queries)

    connect(m_navigator, &QnWorkbenchNavigator::positionChanged, this, &QnCompositeTextOverlay::updateBookmarksFilter);

    /* Update bookmarks by timer to preload new bookmarks smoothly when playing archive. */
    m_updateQueryFilterTimer.reset([this]()
    {
        QTimer *timer = new QTimer();
        timer->setInterval(kBookmarksFilterPrecisionMs / 2);
        connect(timer, &QTimer::timeout, this, &QnCompositeTextOverlay::updateBookmarksFilter);
        timer->start();
        return timer;
    }());

    /* Update bookmarks text by timer. */
    m_updateBookmarksTimer.reset([this]()
    {
        enum { kUpdateBookmarksPeriodMs = 1000 };

        QTimer* timer = new QTimer(this);
        timer->setInterval(kUpdateBookmarksPeriodMs);
        connect(timer, &QTimer::timeout, this, &QnCompositeTextOverlay::updateBookmarks);
        timer->start();
        return timer;
    }());
}

QnCompositeTextOverlayColors QnCompositeTextOverlay::colors() const
{
    return m_colors;
}

void QnCompositeTextOverlay::setColors(const QnCompositeTextOverlayColors &colors)
{
    if (m_colors == colors)
        return;

    m_colors = colors;

    // TODO: #ynikitenkov Update colors on current items
    const auto tmp = m_data[m_currentMode];
    resetModeData(m_currentMode);
    setModeData(m_currentMode, tmp);
}

void QnCompositeTextOverlay::at_modeChanged()
{
    const bool bookmarksEnabled = (mode() == kBookmarksMode);
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

    m_bookmarksQuery->setFilter(constructBookmarksFilter(m_getUtcCurrentTimeMs()));
}

QnOverlayTextItemDataList QnCompositeTextOverlay::removeOutdatedItems(Mode mode)
{
    const auto currentTimestamp = m_counter.elapsed();

    auto &data = m_data[mode];

    typedef QVector<QnUuid> IdsVector;

    IdsVector forRemove;
    const auto lastIt = std::for_each(data.begin(), data.end()
        , [this, &forRemove, currentTimestamp](const InternalData &internalData)
    {
        const QnOverlayTextItemData &itemData = internalData.second;
        const auto timeout = itemData.timeout;
        const auto timestamp = internalData.first;

        const bool timeoutExpired = ((timeout > 0)
            && ((currentTimestamp - timestamp) > timeout));

        if (timeoutExpired)
            forRemove.append(itemData.id);
    });

    for (const auto &id: forRemove)
        data.remove(id);


    QnOverlayTextItemDataList result;
    for (auto &internalData: data)
    {
        QnOverlayTextItemData itemData = internalData.second;
        const auto itemTimestamp = internalData.first;
        auto &itemTimeout = itemData.timeout;
        if (itemTimeout > 0)
            itemTimeout -= (currentTimestamp - itemTimestamp);

        result.append(itemData);
    }
    return result;
}

QnCompositeTextOverlay::InternalDataHash QnCompositeTextOverlay::makeTextItemData(
    const QnCameraBookmarkList &bookmarks
    , const QnBookmarkColors &colors)
{
    enum
    {
        kBorderRadius = 4
        , kPadding = 8
    };

    static QnHtmlTextItemOptions options = QnHtmlTextItemOptions(
        colors.background, false, kBorderRadius, kPadding, kPadding, kMaxItemWidth);

    static const auto kBookTemplate = lit("<html><body>%1%2</body></html>");

    InternalDataHash data;

    const auto timestamp = m_counter.elapsed();
    for (const auto bookmark: bookmarks)
    {
        enum
        {
            kCaptionPixelSize = 16
            , kDescriptionPixeSize = 12
        };

        if (bookmark.name.trimmed().isEmpty() && bookmark.description.trimmed().isEmpty())
            continue;

        const auto bookmarkHtml = kBookTemplate.arg(
            htmlFormattedParagraph(elideString(bookmark.name, kCaptionMaxLength), kCaptionPixelSize, true)
            , htmlFormattedParagraph(elideString(bookmark.description, kDescriptionMaxLength), kDescriptionPixeSize));

        const auto itemData = QnOverlayTextItemData(bookmark.guid, bookmarkHtml, options);
        data.insert(itemData.id, InternalData(timestamp, itemData));
    }
    return data;
};
