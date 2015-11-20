
#include "composite_text_overlay.h"

#include <api/common_message_processor.h>
#include <business/business_strings_helper.h>
#include <business/actions/abstract_business_action.h>

#include <core/resource/camera_resource.h>

namespace
{
    QString makeP(const QString &text
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
}

QnCompositeTextOverlay::QnCompositeTextOverlay(QGraphicsWidget *parent)
    : m_currentMode(kUndefinedMode)
{
}

QnCompositeTextOverlay::~QnCompositeTextOverlay()
{
}

void QnCompositeTextOverlay::init(const QnVirtualCameraResourcePtr &camera)
{
    initTextMode(camera);
}

void QnCompositeTextOverlay::setMode(Mode mode)
{
    if (mode == m_currentMode)
        return;

    m_currentMode = mode;
    setItemsData(m_data[m_currentMode]);
}

void QnCompositeTextOverlay::addModeData(Mode mode
    , const QnOverlayTextItemData &data)
{
    auto &currentData = m_data[mode];
    const auto it = findModeData(mode, data.id);
    if (it == currentData.end())
        currentData.append(data);
    else
        *it = data;

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
    , const QnOverlayTextItemDataList &data)
{
    auto &currentData = m_data[mode];
    if (currentData == data)
        return;

    m_data[mode] = data;
    if (m_currentMode == mode)
        setItemsData(data);
}

void QnCompositeTextOverlay::resetModeData(Mode mode)
{
    setModeData(mode, QnOverlayTextItemDataList());
}

QnOverlayTextItemDataList::Iterator QnCompositeTextOverlay::findModeData(Mode mode
    , const QnUuid &id)
{
    auto &currentData = m_data[mode];
    const auto it = std::find_if(currentData.begin(), currentData.end()
        , [id](const QnOverlayTextItemData &data)
    {
        return (data.id == id);
    });
    return it;
}

void QnCompositeTextOverlay::initTextMode(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    const auto cameraId = camera->getId();
    const auto messageProcessor = QnCommonMessageProcessor::instance();

    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived
        , this, [this, cameraId](const QnAbstractBusinessActionPtr &businessAction)
    {
        qDebug() << "--- businessActionReceived " << businessAction->actionType();
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

            qDebug() << "Remove action " << actionId;
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
            const auto caption = QnBusinessStringsHelper::eventAtResource(runtimeParams, true);
            const auto desciption = QnBusinessStringsHelper::eventDetails(runtimeParams, lit("\n"));

            static const auto kComplexHtml = lit("<html><body>%1%2</body></html>");
            text = kComplexHtml.arg(makeP(caption, 16, true)
                , makeP(desciption, 13));
        }
        else
        {
            static const auto kTextHtml = lit("<html><body>%1</body></html>");
            text = kTextHtml.arg(actionParams.text);
        }

        const QnHtmlTextItemOptions options(QColor(0, 0, 0, 0x80), true, 2, 8, 250);

        const QnOverlayTextItemData data(actionId, text, options, timeout);
        addModeData(QnCompositeTextOverlay::kTextAlaramsMode, data);
        qDebug() << "Add action " << actionId << " : " << isInstantAction << ":" << actionParams.durationMs
            << " : " << businessAction->getToggleState();

    });
}

