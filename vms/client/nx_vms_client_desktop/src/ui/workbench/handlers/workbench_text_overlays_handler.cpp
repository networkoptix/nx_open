// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_text_overlays_handler.h"

#include <algorithm>
#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtGui/QTextDocument>

#include <client/client_message_processor.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/actions/text_overlay_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/utils/action.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/delayed.h>

using namespace nx;
using namespace nx::vms::client::desktop;

namespace {

using namespace std::chrono;

static constexpr auto kMinimumTextOverlayDuration = 5s;

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

QString formatOverlayText(
    const QString& text,
    const QString& rawCaption,
    const QStringList& rawDescription)
{
    using namespace nx::vms::common;

    const QString kHtmlPageTemplate =
        "<html><head><style>* {text-indent: 0; margin-top: 0; "
        "margin-bottom: 0; margin-left: 0; margin-right: 0; "
        "color: white;}</style></head><body>%1</body></html>";

    constexpr int kCaptionMaxLength = 64;
    constexpr int kDescriptionMaxLength = 512;
    constexpr int kCaptionPixelFontSize = 16;
    const auto kDescriptionPixelFontSize = fontConfig()->normal().pixelSize();

    QString textHtml;

    if (text.isEmpty())
    {
        const auto caption = html::mightBeHtml(rawCaption)
            ? rawCaption
            : rawCaption.toHtmlEscaped();

        // NewLine symbols will be replaced with html tag later.
        const auto description = html::mightBeHtml(rawDescription)
            ? rawDescription.join(html::kLineBreak)
            : rawDescription.join('\n').toHtmlEscaped();

        const auto captionHtml = html::styledParagraph(
            caption,
            kCaptionPixelFontSize,
            /*bold*/ true);
        const auto descriptionHtml = html::styledParagraph(
            description,
            kDescriptionPixelFontSize);

        textHtml = kHtmlPageTemplate.arg(
            html::elide(captionHtml, kCaptionMaxLength) +
            html::elide(descriptionHtml, kDescriptionMaxLength));
    }
    else
    {
        textHtml = html::elide(
            html::styledParagraph(html::toHtml(text), kDescriptionPixelFontSize),
            kDescriptionMaxLength);
    }

    return textHtml;
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

    void showTextOverlays(
        const QnResourcePtr& resource,
        QnUuid id,
        const QString& text,
        std::chrono::milliseconds timeout);

    void hideTextOverlays(const QnResourcePtr& resource, const QnUuid& id);

    void handleNewWidget(QnResourceWidget* widget);

private:
    struct OverlayData
    {
        QString text;
        std::chrono::milliseconds timeout = -1ms;
        QElapsedTimer elapsedTimer;
        QTimer* autohideTimer = nullptr;
    };

    void hideImmediately(const QnResourcePtr& resource, const QnUuid& id);

    void setupAutohideTimer(
        OverlayData& data,
        const QnResourcePtr& resource,
        QnUuid id,
        std::chrono::milliseconds timeout);

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
    m_helper(new nx::vms::event::StringsHelper(systemContext()))
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::businessActionReceived,
        this, &QnWorkbenchTextOverlaysHandler::at_eventActionReceived);

    connect(display(), &QnWorkbenchDisplay::widgetAdded,
        this, &QnWorkbenchTextOverlaysHandler::at_resourceWidgetAdded);

    systemContext()->vmsRulesEngine()->addActionExecutor(
        nx::vms::rules::TextOverlayAction::manifest().id,
        this);
}

QnWorkbenchTextOverlaysHandler::~QnWorkbenchTextOverlaysHandler()
{
}

void QnWorkbenchTextOverlaysHandler::at_eventActionReceived(
    const vms::event::AbstractActionPtr& businessAction)
{
    if (businessAction->actionType() != vms::api::ActionType::showTextOverlayAction)
        return;

    if (!context()->user())
        return;

    const auto& actionParams = businessAction->getParams();

    const auto state = businessAction->getToggleState();
    const bool isProlongedAction = businessAction->isProlonged();
    const bool couldBeInstantEvent = (state == vms::api::EventState::undefined);

    /* Do not accept instant events for prolonged actions. */
    if (isProlongedAction && couldBeInstantEvent)
        return;

    auto cameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        businessAction->getResources());

    if (actionParams.useSource)
    {
        cameras << resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
            businessAction->getSourceResources(resourcePool()));
    }

    /* Remove duplicates. */
    std::sort(cameras.begin(), cameras.end());
    cameras.erase(std::unique(cameras.begin(), cameras.end()), cameras.end());

    const auto timeout = std::chrono::milliseconds(
        isProlongedAction ? -1 : actionParams.durationMs);

    const auto id = businessAction->getRuleId();
    const bool finished = isProlongedAction && (state == vms::api::EventState::inactive);

    Q_D(QnWorkbenchTextOverlaysHandler);

    if (finished)
    {
        for (const auto& camera: cameras)
            d->hideTextOverlays(camera, id);
    }
    else
    {
        const QString text = actionParams.text.trimmed();
        const auto runtimeParams = businessAction->getRuntimeParams();
        const auto rawCaption = m_helper->eventAtResource(runtimeParams, Qn::RI_WithUrl);
        const auto rawDescription = m_helper->eventDetails(
            runtimeParams,
            nx::vms::event::AttrSerializePolicy::singleLine);

        const QString textHtml = formatOverlayText(text, rawCaption, rawDescription);

        for (const auto& camera: cameras)
            d->showTextOverlays(camera, id, textHtml, timeout);
    }
}

void QnWorkbenchTextOverlaysHandler::at_resourceWidgetAdded(QnResourceWidget* widget)
{
    Q_D(QnWorkbenchTextOverlaysHandler);
    d->handleNewWidget(widget);
}

void QnWorkbenchTextOverlaysHandler::execute(const nx::vms::rules::ActionPtr& action)
{
    const auto overlayAction = action.dynamicCast<nx::vms::rules::TextOverlayAction>();
    if (!NX_ASSERT(overlayAction))
        return;

    if (!nx::vms::rules::checkUserPermissions(context()->user(), action))
        return;

    const auto ruleId = overlayAction->ruleId();
    const auto cameras =
        resourcePool()->getResourcesByIds<QnVirtualCameraResource>(overlayAction->deviceIds());

    Q_D(QnWorkbenchTextOverlaysHandler);

    if (overlayAction->state() == nx::vms::rules::State::stopped)
    {
        for (const auto& camera: cameras)
            d->hideTextOverlays(camera, ruleId);
    }
    else
    {
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            overlayAction->duration());

        const QString textHtml = formatOverlayText(
            overlayAction->text().trimmed(),
            overlayAction->extendedCaption(),
            overlayAction->detailing());

        for (const auto& camera: cameras)
            d->showTextOverlays(camera, ruleId, textHtml, duration);
    }
}

/*
* QnWorkbenchTextOverlaysHandlerPrivate implementation
*/

void QnWorkbenchTextOverlaysHandlerPrivate::showTextOverlays(
    const QnResourcePtr& resource,
    QnUuid id,
    const QString& text,
    std::chrono::milliseconds timeout)
{
    Q_Q(QnWorkbenchTextOverlaysHandler);

    for (auto widget: q->display()->widgets(resource))
    {
        if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
            mediaWidget->showTextOverlay(id, text, textOverlayOptions());
    }

    auto& data = m_overlays[resource][id];
    data.text = text;
    data.timeout = timeout;
    data.elapsedTimer.start();

    setupAutohideTimer(data, resource, id, timeout); //< Will clear timer if timeout <= 0.
}

void QnWorkbenchTextOverlaysHandlerPrivate::hideTextOverlays(
    const QnResourcePtr& resource, const QnUuid& id)
{
    auto data = findData(resource, id);
    if (!data)
        return;

    const auto remaining = data->elapsedTimer.isValid()
        ? kMinimumTextOverlayDuration - std::chrono::milliseconds(data->elapsedTimer.elapsed())
        : 0ms;

    if (remaining.count() > 0)
        setupAutohideTimer(*data, resource, id, remaining);
    else
        hideImmediately(resource, id);
}

void QnWorkbenchTextOverlaysHandlerPrivate::handleNewWidget(QnResourceWidget* widget)
{
    NX_ASSERT(widget);
    if (!widget)
        return;

    const auto resourceIter = m_overlays.find(widget->resource());
    if (resourceIter == m_overlays.end())
        return;

    auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
    NX_ASSERT(mediaWidget);
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

void QnWorkbenchTextOverlaysHandlerPrivate::setupAutohideTimer(
    OverlayData& data,
    const QnResourcePtr& resource,
    QnUuid id,
    std::chrono::milliseconds timeout)
{
    Q_Q(QnWorkbenchTextOverlaysHandler);

    if (data.autohideTimer)
    {
        data.autohideTimer->stop();
        data.autohideTimer->deleteLater();
        data.autohideTimer = nullptr;
    }

    if (timeout.count() > 0)
    {
        data.autohideTimer = executeDelayedParented(
            [this, resource, id] { hideImmediately(resource, id); },
            timeout, q);
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
    setupAutohideTimer(dataIter.value(), resource, id, -1ms);

    resourceOverlays.erase(dataIter);

    if (resourceOverlays.empty())
        m_overlays.erase(resourceIter);
}
