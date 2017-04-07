#include "workbench_text_overlays_handler.h"

#include <algorithm>

#include <business/actions/abstract_business_action.h>
#include <business/business_strings_helper.h>

#include <client/client_message_processor.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/html.h>

QnWorkbenchTextOverlaysHandler::QnWorkbenchTextOverlaysHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::businessActionReceived,
        this, &QnWorkbenchTextOverlaysHandler::at_businessActionReceived);
}

QnWorkbenchTextOverlaysHandler::~QnWorkbenchTextOverlaysHandler()
{
}

void QnWorkbenchTextOverlaysHandler::at_businessActionReceived(
    const QnAbstractBusinessActionPtr& businessAction)
{
    if (businessAction->actionType() != QnBusiness::ShowTextOverlayAction)
        return;

    if (!context()->user())
        return;

    const auto& actionParams = businessAction->getParams();

    const auto state = businessAction->getToggleState();
    const bool isProlongedAction = businessAction->isProlonged();
    const bool couldBeInstantEvent = (state == QnBusiness::UndefinedState);

    /* Do not accept instant events for prolonged actions. */
    if (isProlongedAction && couldBeInstantEvent)
        return;

    auto cameras = qnResPool->getResources<QnVirtualCameraResource>(
        businessAction->getResources());

    if (actionParams.useSource)
    {
        cameras << qnResPool->getResources<QnVirtualCameraResource>(
            businessAction->getSourceResources());
    }

    /* Remove duplicates. */
    std::sort(cameras.begin(), cameras.end());
    cameras.erase(std::unique(cameras.begin(), cameras.end()), cameras.end());

    const int timeoutMs = isProlongedAction ? -1 : actionParams.durationMs;

    const auto id = businessAction->getBusinessRuleId();
    const bool finished = isProlongedAction && (state == QnBusiness::InactiveState);

    if (finished)
    {
        for (const auto& camera: cameras)
        {
            for (auto widget: display()->widgets(camera))
            {
                if (auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget))
                    mediaWidget->hideTextOverlay(id);
            }
        }
    }
    else
    {
        for (const auto& camera: cameras)
        {
            for (auto widget: display()->widgets(camera))
            {
                auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
                if (!mediaWidget)
                    continue;

                QString caption;
                QString description = actionParams.text.trimmed();

                if (description.isEmpty())
                {
                    const auto runtimeParams = businessAction->getRuntimeParams();
                    caption = QnBusinessStringsHelper::eventAtResource(runtimeParams, Qn::RI_WithUrl);
                    description = QnBusinessStringsHelper::eventDetails(runtimeParams).join(L'\n');
                }

                showTextOverlay(mediaWidget, id, caption, description, timeoutMs);
            }
        }
    }
}

void QnWorkbenchTextOverlaysHandler::showTextOverlay(QnMediaResourceWidget* widget,
    const QnUuid& id, const QString& captionHtml, const QString& descriptionHtml, int timeoutMs)
{
    const auto kHtmlPageTemplate = lit(
        "<html><head><style>* {text-ident: 0; margin-top: 0; "
        "margin-bottom: 0; margin-left: 0; margin-right: 0; "
        "color: white;}</style></head><body>%1</body></html>");

    const auto kComplexHtml = lit("%1%2");

    static constexpr int kCaptionMaxLength = 64;
    static constexpr int kDescriptionMaxLength = 160;
    static constexpr int kMaxItemWidth = 250;
    static constexpr int kDescriptionPixelFontSize = 13;
    static constexpr int kCaptionPixelFontSize = 16;
    static constexpr int kHorPaddings = 12;
    static constexpr int kVertPaddings = 8;
    static constexpr int kBorderRadius = 2;

    const auto caption = htmlFormattedParagraph(captionHtml, kCaptionPixelFontSize, true);
    const auto description = htmlFormattedParagraph(descriptionHtml, kDescriptionPixelFontSize);

    const QString text = kHtmlPageTemplate.arg(kComplexHtml.arg(
        elideHtml(caption, kCaptionMaxLength),
        elideHtml(description, kDescriptionMaxLength)));
    /*
    Currently background color is not explicitly specified and effectively
    falls back to QnResourceHudOverlay::palette().color(QPalette::Window).
    */
    const QnHtmlTextItemOptions options(QColor(), true,
        kBorderRadius, kHorPaddings, kVertPaddings, kMaxItemWidth);

    widget->showTextOverlay(id, text, options, timeoutMs);
}
