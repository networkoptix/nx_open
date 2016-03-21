
#include "composite_text_overlay.h"

#include <api/common_message_processor.h>

#include <utils/common/string.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include <business/business_strings_helper.h>
#include <business/actions/abstract_business_action.h>

#include <ui/workbench/workbench_navigator.h>
#include <ui/common/search_query_strategy.h>
#include <ui/common/search_query_strategy.h>

namespace
{
    const auto kHtmlPageTemplate = lit("<html><head><style>* {text-ident: 0; margin-top: 0; margin-bottom: 0; margin-left: 0; margin-right: 0; color: white;}</style></head><body>%1</body></html>");
    const auto kComplexHtml = lit("%1%2");

    enum
    {
        kCaptionMaxLength = 64
        , kDescriptionMaxLength = 160
        , kMaxItemWidth = 250
    };
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

    , m_colors()
{
    m_counter.start();
    initTextMode();
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

        const auto state = businessAction->getToggleState();
        // Text Overlay actions with defined duration are for instant events,
        // otherwise they are for prolonged.
        const bool isProlongedAction = (actionParams.durationMs <= 0);

        const bool couldBeInstantEvent = (state == QnBusiness::UndefinedState);
        if (isProlongedAction && couldBeInstantEvent)
        {
            // Do not accept instant events for prolonged actions
            return;
        }

        const int timeout = (isProlongedAction ? QnOverlayTextItemData::kInfinite
            : actionParams.durationMs);

        const auto actionId = businessAction->getBusinessRuleId();

        if (isProlongedAction && (state == QnBusiness::InactiveState))
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

            const auto caption = QnBusinessStringsHelper::eventAtResource(runtimeParams, true);
            const auto htmlCaption = htmlFormattedParagraph(caption, kCaptionPixelFontSize, true);

            const auto desc = QnBusinessStringsHelper::eventDetails(runtimeParams, lit("\n"));
            const auto htmlDesc = htmlFormattedParagraph(desc, kDescriptionPixelFontSize);

            if (caption.trimmed().isEmpty() && desc.trimmed().isEmpty())  // Do not add empty text items
                return;

            text = kComplexHtml.arg(elideHtml(htmlCaption, kCaptionMaxLength)
                , elideHtml(htmlDesc, kDescriptionMaxLength));

            text = kHtmlPageTemplate.arg(text);
        }
        else
        {
            if (actionParams.text.trimmed().isEmpty()) // Do not add empty text items
                return;

            const auto elided = elideHtml(htmlFormattedParagraph(
                actionParams.text, kDescriptionPixelFontSize), kDescriptionMaxLength);
            text = kHtmlPageTemplate.arg(elided);
        }

        const QnHtmlTextItemOptions options(m_colors.textOverlayItemColor, true
            , kBorderRadius, kHorPaddings, kVertPaddings, kMaxItemWidth);

        const QnOverlayTextItemData data(actionId, text, options, timeout);
        addModeData(QnCompositeTextOverlay::kTextOutputMode, data);
    });
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
