#include "analytics_action_factory.h"

#include <QtWidgets/QAction>

#include <common/common_module.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_conditions.h>
#include <nx/client/desktop/analytics/drivers/analytics_drivers_factory.h>
#include <nx/client/desktop/layout_templates/layout_template_manager.h>

namespace nx {
namespace client {
namespace desktop {

AnalyticsActionFactory::AnalyticsActionFactory(QObject* parent):
    base_type(parent)
{
}

QList<QAction*> AnalyticsActionFactory::newActions(const ui::action::Parameters& parameters,
    QObject* parent)
{
    QList<QAction*> result;

    static const int kMinMatrixSize = 2;
    static const int kMaxMatrixSize = 4;

    for (int i = kMinMatrixSize; i <= kMaxMatrixSize; ++i)
    {
        auto action = new QAction(parent);
        action->setText(lit("%1x%1").arg(i));

        connect(action, &QAction::triggered, this,
            [this, parameters, i]
            {
                menu()->trigger(ui::action::StartAnalyticsAction,
                    ui::action::Parameters(parameters).withArgument(Qn::IntRole, i));
            });

        result << action;
    }

    {
        auto action = new QAction(parent);
        action->setText(tr("Dynamic"));
        connect(action, &QAction::triggered, this,
            [this, parameters]
            {
                menu()->trigger(ui::action::StartAnalyticsAction,
                    ui::action::Parameters(parameters));
            });

        result << action;
    }

    if (const auto templatesManager = commonModule()->findInstance<LayoutTemplateManager>())
    {
        for (const auto& layoutTemplate: templatesManager->templates())
        {
            if (layoutTemplate.type == LayoutTemplate::kAnalyticsLayoutType)
            {
                const auto action = new QAction(parent);
                action->setText(layoutTemplate.name);
                connect(action, &QAction::triggered, this,
                    [this, parameters, layoutTemplate]
                    {
                        menu()->trigger(
                            ui::action::StartAnalyticsAction,
                            ui::action::Parameters(parameters)
                                .withArgument(
                                    Qn::LayoutTemplateRole,
                                    QVariant::fromValue(layoutTemplate)));
                    });

                result << action;
            }
        }
    }

    return result;
}

ui::action::ConditionWrapper AnalyticsActionFactory::condition()
{
    return new ui::action::ResourceCondition(
        [](const QnResourcePtr& resource)
        {
            return AnalyticsDriversFactory::supportsAnalytics(resource);
        },
        MatchMode::All);
}

} // namespace desktop
} // namespace client
} // namespace nx
