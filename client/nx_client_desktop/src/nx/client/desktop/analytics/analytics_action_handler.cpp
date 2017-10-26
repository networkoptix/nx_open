#include "analytics_action_handler.h"

#include <QtCore/QTimer>

#include <ini.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/analytics/drivers/analytics_drivers_factory.h>
#include <nx/client/desktop/analytics/workbench_analytics_controller.h>
#include <nx/client/desktop/layout_templates/layout_template.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/random.h>

namespace nx {
namespace client {
namespace desktop {

AnalyticsActionHandler::AnalyticsActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    if (ini().enableAnalytics)
    {
        connect(action(ui::action::StartAnalyticsAction), &QAction::triggered, this,
            &AnalyticsActionHandler::startAnalytics);
    }

    connect(workbench(), &QnWorkbench::layoutsChanged, this,
        &AnalyticsActionHandler::cleanupControllers);
}

AnalyticsActionHandler::~AnalyticsActionHandler()
{
}

void AnalyticsActionHandler::startAnalytics()
{
    auto parameters = menu()->currentParameters(sender());
    const auto resource = parameters.resource();
    const auto driver = AnalyticsDriversFactory::createAnalyticsDriver(resource);
    if (!driver)
        return;

    const auto& layoutTemplate =
        parameters.argument(Qn::LayoutTemplateRole).value<LayoutTemplate>();

    int size = parameters.argument(Qn::IntRole).toInt();

    auto existing = std::find_if(m_controllers.cbegin(), m_controllers.cend(),
        [resource, size, templateName = layoutTemplate.name](const ControllerPtr& controller)
        {
            if (controller->resource() != resource)
                return false;

            if (!templateName.isEmpty())
                return controller->layoutTemplate().name == templateName;

            return controller->matrixSize() == size;
        });

    if (existing != m_controllers.cend())
    {
        auto layout = (*existing)->layout();
        if (auto existingLayout = QnWorkbenchLayout::instance(layout))
            workbench()->setCurrentLayout(existingLayout);
        else
            menu()->trigger(ui::action::OpenInNewTabAction, layout); //< OpenInSingleLayoutAction
        return;
    }

    const ControllerPtr controller(layoutTemplate.isValid()
        ? new WorkbenchAnalyticsController(layoutTemplate, resource, driver, this)
        : new WorkbenchAnalyticsController(size, resource, driver, this));

    m_controllers.append(controller);
    menu()->trigger(ui::action::OpenInNewTabAction, controller->layout());
}

void AnalyticsActionHandler::cleanupControllers()
{
    QSet<QnLayoutResourcePtr> usedLayouts;
    for (auto layout: workbench()->layouts())
        usedLayouts << layout->resource();

    for (auto iter = m_controllers.begin(); iter != m_controllers.end();)
    {
        auto layout = (*iter)->layout();
        if (usedLayouts.contains(layout))
            ++iter;
        else
            iter = m_controllers.erase(iter);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
