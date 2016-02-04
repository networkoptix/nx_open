
#include "actions_statistics_module.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/statistics/private/action_metrics.h>

namespace
{
    QString aliasByActionId(Qn::ActionId id)
    {
        typedef QHash<Qn::ActionId, QString> AliasesHash;
        static const auto aliases = []() -> AliasesHash
        {
            AliasesHash result;
            result.insert(Qn::BookmarksModeAction, lit("bookmark_mode"));
            // TODO: add aliases values
            return result;
        }();


        const auto it = aliases.find(id);
        if (it == aliases.end())
        {
            //Q_ASSERT_X(false, Q_FUNC_INFO, "Unregistered action couldn't be used for statistics extraction");
            static int unregisterdCounter = 0;
            return lit("unregisterd_action_%1").arg(++unregisterdCounter);
        }
        return *it;
    }

    QString makeAlias(const QString &alias, const QString &postfix)
    {
        return lit("%1_%2").arg(alias, postfix);
    };
}

QnActionsStatisticsModule::QnActionsStatisticsModule(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    const auto createActionMetrics = [this](Qn::ActionId id)
    {
        const auto &alias = aliasByActionId(id);

        const auto triggeredAlias = makeAlias(alias, ActionTriggeredCountMetric::kPostfix);
        m_metrics[triggeredAlias] = MetricsPtr(new ActionTriggeredCountMetric(menu(), id));

        // TODO: Add metrics handler
    };

    connect(menu(), &QnActionManager::actionRegistered, this, createActionMetrics);

    for (const auto action: menu()->actions())
        createActionMetrics(action->id());
}

QnActionsStatisticsModule::~QnActionsStatisticsModule()
{
}

QnMetricsHash QnActionsStatisticsModule::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_metrics.cbegin(); it != m_metrics.cend(); ++it)
    {
        const auto alias = it.key();
        const auto subMetrics = it.value()->metrics();
        for (auto itSub = subMetrics.cbegin(); itSub != subMetrics.cend(); ++itSub)
            result.insert(alias, itSub.value());
    }
    return result;
}

void QnActionsStatisticsModule::resetMetrics()
{
    for (const auto &metric: m_metrics)
        metric->reset();
}
