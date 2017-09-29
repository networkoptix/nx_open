#include "workbench_text_overlays_handler.h"

#include <algorithm>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <QtGui/QTextDocument>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

#include <client/client_message_processor.h>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/uuid.h>

#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/delayed.h>
#include <utils/common/html.h>

using namespace nx;

namespace {

static constexpr int kMinimumTextOverlayDurationMs = 5000;

QnHtmlTextItemOptions textOverlayOptions()
{
    static constexpr int kMaxItemWidth = 250;
    static constexpr int kHorPaddings = 12;
    static constexpr int kVertPaddings = 8;
    static constexpr int kBorderRadius = 2;

    /* Currently background color is not explicitly specified and effectively
     * falls back to QnResourceHudOverlay::palette().color(QPalette::Window). */
    return QnHtmlTextItemOptions(QColor(), true,
        kBorderRadius, kHorPaddings, kVertPaddings, kMaxItemWidth);
}

} // namespace

/*
* QnWorkbenchTextOverlaysHandlerPrivate declaration
*/

class QnWorkbenchTextOverlaysHandlerPrivate
{
    Q_DECLARE_PUBLIC(QnWorkbenchTextOverlaysHandler);
    QnWorkbenchTextOverlaysHandler* const q_ptr;

public:
    QnWorkbenchTextOverlaysHandlerPrivate(QnWorkbenchTextOverlaysHandler* main): q_ptr(main) {}

    void showTextOverlays(const QnResourcePtr& resource, const QnUuid& id,
        const QString& text, int timeoutMs);

    void hideTextOverlays(const QnResourcePtr& resource, const QnUuid& id);

    void handleNewWidget(QnResourceWidget* widget);

private:
    struct OverlayData
    {
        QString text;
        int timeoutMs = -1;
        QElapsedTimer elapsedTimer;
        QTimer* autohideTimer = nullptr;
    };

    void hideImmediately(const QnResourcePtr& resource, const QnUuid& id);

    void setupAutohideTimer(OverlayData& data,
        const QnResourcePtr& resource, const QnUuid& id, int timeoutMs);

    OverlayData* findData(const QnResourcePtr& resource, const QnUuid& id);
    void removeData(const QnResourcePtr& resource, const QnUuid& id);

private:
    using ResourceOverlays = QHash<QnUuid, OverlayData>;
    using AllOverlays = QHash<QnResourcePtr, ResourceOverlays>;

    AllOverlays m_overlays;
};

/*
* QnWorkbenchTextOverlaysHandler implementation
*/

QnWorkbenchTextOverlaysHandler::QnWorkbenchTextOverlaysHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnWorkbenchTextOverlaysHandlerPrivate(this)),
    m_helper(new nx::vms::event::StringsHelper(commonModule()))
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::businessActionReceived,
        this, &QnWorkbenchTextOverlaysHandler::at_eventActionReceived);

    connect(display(), &QnWorkbenchDisplay::widgetAdded,
        this, &QnWorkbenchTextOverlaysHandler::at_resourceWidgetAdded);
}

QnWorkbenchTextOverlaysHandler::~QnWorkbenchTextOverlaysHandler()
{
}

void QnWorkbenchTextOverlaysHandler::at_eventActionReceived(
    const vms::event::AbstractActionPtr& businessAction)
{
    if (businessAction->actionType() != vms::event::showTextOverlayAction)
        return;

    if (!context()->user())
        return;

    const auto& actionParams = businessAction->getParams();

    const auto state = businessAction->getToggleState();
    const bool isProlongedAction = businessAction->isProlonged();
    const bool couldBeInstantEvent = (state == vms::event::EventState::undefined);

    /* Do not accept instant events for prolonged actions. */
    if (isProlongedAction && couldBeInstantEvent)
        return;

    auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(
        businessAction->getResources());

    if (actionParams.useSource)
    {
        cameras << resourcePool()->getResources<QnVirtualCameraResource>(
            businessAction->getSourceResources());
    }

    /* Remove duplicates. */
    std::sort(cameras.begin(), cameras.end());
    cameras.erase(std::unique(cameras.begin(), cameras.end()), cameras.end());

    const int timeoutMs = isProlongedAction ? -1 : actionParams.durationMs;

    const auto id = businessAction->getRuleId();
    const bool finished = isProlongedAction && (state == vms::event::EventState::inactive);

    Q_D(QnWorkbenchTextOverlaysHandler);

    if (finished)
    {
        for (const auto& camera: cameras)
            d->hideTextOverlays(camera, id);
    }
    else
    {
        const auto kHtmlPageTemplate = lit(
            "<html><head><style>* {text-ident: 0; margin-top: 0; "
            "margin-bottom: 0; margin-left: 0; margin-right: 0; "
            "color: white;}</style></head><body>%1</body></html>");

        static constexpr int kCaptionMaxLength = 64;
        static constexpr int kDescriptionMaxLength = 160;
        static constexpr int kCaptionPixelFontSize = 16;
        static constexpr int kDescriptionPixelFontSize = 13;

        QString textHtml;
        const QString text = actionParams.text.trimmed();

        if (text.isEmpty())
        {
            const auto runtimeParams = businessAction->getRuntimeParams();
            const auto rawCaption = m_helper->eventAtResource(runtimeParams, Qn::RI_WithUrl);
            const auto caption = mightBeHtml(rawCaption)
                ? rawCaption
                : rawCaption.toHtmlEscaped();

            // NewLine symbols will be replaced with html tag later.
            const auto rawDescription = m_helper->eventDetails(runtimeParams);
            const auto description = mightBeHtml(rawDescription)
                ? rawDescription.join(lit("<br>"))
                : rawDescription.join(L'\n').toHtmlEscaped();

            const auto captionHtml = htmlFormattedParagraph(caption, kCaptionPixelFontSize, true);
            const auto descriptionHtml = htmlFormattedParagraph(description, kDescriptionPixelFontSize);

            const auto kComplexHtml = lit("%1%2");
            textHtml = kHtmlPageTemplate.arg(kComplexHtml.arg(
                elideHtml(captionHtml, kCaptionMaxLength),
                elideHtml(descriptionHtml, kDescriptionMaxLength)));
        }
        else
        {
            textHtml = elideHtml(
                htmlFormattedParagraph(ensureHtml(text), kDescriptionPixelFontSize),
                kDescriptionMaxLength);
        }

        for (const auto& camera: cameras)
            d->showTextOverlays(camera, id, textHtml, timeoutMs);
    }
}

void QnWorkbenchTextOverlaysHandler::at_resourceWidgetAdded(QnResourceWidget* widget)
{
    Q_D(QnWorkbenchTextOverlaysHandler);
    d->handleNewWidget(widget);
}

/*
* QnWorkbenchTextOverlaysHandlerPrivate implementation
*/

void QnWorkbenchTextOverlaysHandlerPrivate::showTextOverlays(
    const QnResourcePtr& resource, const QnUuid& id,
    const QString& text, int timeoutMs)
{
    Q_Q(QnWorkbenchTextOverlaysHandler);

    for (auto widget: q->display()->widgets(resource))
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
            mediaWidget->showTextOverlay(id, text, textOverlayOptions());
    }

    auto& data = m_overlays[resource][id];
    data.text = text;
    data.timeoutMs = timeoutMs;
    data.elapsedTimer.start();

    setupAutohideTimer(data, resource, id, timeoutMs); //< will clear timer if timeoutMs < 0
}

void QnWorkbenchTextOverlaysHandlerPrivate::hideTextOverlays(
    const QnResourcePtr& resource, const QnUuid& id)
{
    auto data = findData(resource, id);
    if (!data)
        return;

    const qint64 remainingMs = data->elapsedTimer.isValid()
        ? kMinimumTextOverlayDurationMs - data->elapsedTimer.elapsed()
        : qint64(0);

    if (remainingMs > 0)
        setupAutohideTimer(*data, resource, id, static_cast<int>(remainingMs));
    else
        hideImmediately(resource, id);
}

void QnWorkbenchTextOverlaysHandlerPrivate::handleNewWidget(QnResourceWidget* widget)
{
    NX_EXPECT(widget);
    if (!widget)
        return;

    const auto resourceIter = m_overlays.find(widget->resource());
    if (resourceIter == m_overlays.end())
        return;

    auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
    NX_EXPECT(mediaWidget);
    if (!mediaWidget)
        return;

    const auto& resourceOverlays = resourceIter.value();

    for (auto iter = resourceOverlays.cbegin(); iter != resourceOverlays.cend(); ++iter)
        mediaWidget->showTextOverlay(iter.key(), iter.value().text, textOverlayOptions());
}

void QnWorkbenchTextOverlaysHandlerPrivate::hideImmediately(
    const QnResourcePtr& resource, const QnUuid& id)
{
    Q_Q(QnWorkbenchTextOverlaysHandler);
    for (auto widget: q->display()->widgets(resource))
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
            mediaWidget->hideTextOverlay(id);
    }

    removeData(resource, id);
}

void QnWorkbenchTextOverlaysHandlerPrivate::setupAutohideTimer(OverlayData& data,
    const QnResourcePtr& resource, const QnUuid& id, int timeoutMs)
{
    Q_Q(QnWorkbenchTextOverlaysHandler);

    if (data.autohideTimer)
    {
        data.autohideTimer->stop();
        data.autohideTimer->deleteLater();
        data.autohideTimer = nullptr;
    }

    if (timeoutMs > 0)
    {
        data.autohideTimer = executeDelayedParented(
            [this, resource, id] { hideImmediately(resource, id); },
            timeoutMs, q);
    }
}

QnWorkbenchTextOverlaysHandlerPrivate::OverlayData*
    QnWorkbenchTextOverlaysHandlerPrivate::findData(
        const QnResourcePtr& resource,
        const QnUuid& id)
{
    auto resourceIter = m_overlays.find(resource);
    if (resourceIter == m_overlays.end())
        return nullptr;

    auto& resourceOverlays = resourceIter.value();

    auto dataIter = resourceOverlays.find(id);
    if (dataIter == resourceOverlays.end())
        return nullptr;

    return &dataIter.value();
}

void QnWorkbenchTextOverlaysHandlerPrivate::removeData(
    const QnResourcePtr& resource, const QnUuid& id)
{
    auto resourceIter = m_overlays.find(resource);
    if (resourceIter == m_overlays.end())
        return;

    auto& resourceOverlays = resourceIter.value();

    auto dataIter = resourceOverlays.find(id);
    if (dataIter == resourceOverlays.end())
        return;

    /* Clear auto-hide timer: */
    setupAutohideTimer(dataIter.value(), resource, id, -1);

    resourceOverlays.erase(dataIter);

    if (resourceOverlays.empty())
        m_overlays.erase(resourceIter);
}
