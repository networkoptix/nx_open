
#include "action_metrics.h"

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <statistics/base/metrics_container.h>
#include <statistics/base/time_duration_metric.h>

#include <nx/fusion/model_functions.h>

using namespace nx::client::desktop::ui;

namespace
{
    template<typename Type>
    QPointer<Type> makePointer(Type *value)
    {
        return QPointer<Type>(value);
    }

    QString aliasByActionId(action::IDType id)
    {
        return QnLexical::serialized(id);
    }

    QString makeAlias(const QString &alias, const QString &postfix)
    {
        return lit("%1_%2").arg(alias, postfix);
    };

    QString makePath(const QString &baseTag
        , const QObject *source)
    {
        if (!source)
            return baseTag;

        QStringList tagsList(baseTag);

        const auto &name = source->objectName();
        if (!name.isEmpty())
            tagsList.append(name);

        const auto className = QString::fromLatin1(source->staticMetaObject.className());
        if (!className.isEmpty() && (className != QLatin1String("QObject")))
            tagsList.append(className);

        return tagsList.join(L'_');
    }
}

namespace
{
    class ActionDurationMetric : public QnTimeDurationMetric
        , public QObject
    {
        typedef QnTimeDurationMetric base_type;
    public:
        ActionDurationMetric(action::Action* action);

        virtual ~ActionDurationMetric();
    private:
    };

    ActionDurationMetric::ActionDurationMetric(action::Action* action)
        : base_type()
        , QObject()
    {
        if (!action)
            return;

        const auto id = action->id();
        const auto guard = makePointer(this);
        const auto processToggled = [this, guard, id](bool checked)
        {
            if (!guard)
                return;

            // TODO: remove debug output, id and unnecessary condition
            if (isCounterActive() == checked)
                return;

            setCounterActive(checked);
        };

        QObject::connect(action, &QAction::toggled, this, processToggled);
        processToggled(action->isChecked());
    }

    ActionDurationMetric::~ActionDurationMetric()
    {}
}

//

AbstractActionsMetrics::AbstractActionsMetrics(const action::ManagerPtr& actionManager)
    : base_type()
    , QnStatisticsValuesProvider()
{
    if (!actionManager)
        return;

    const auto guard = makePointer(this);
    const auto addAction = [this, guard, actionManager](action::IDType id)
    {
        if (!guard)
            return;

        const auto action = actionManager->action(id);
        if (!action)
            return;

        addActionMetric(action);
    };

    connect(actionManager.data(), &action::Manager::actionRegistered, this, addAction);
}

AbstractActionsMetrics::~AbstractActionsMetrics()
{}

//

ActionsTriggeredCountMetrics::ActionsTriggeredCountMetrics(const action::ManagerPtr& actionManager)
    : base_type(actionManager)
    , m_values()
{
    if (!actionManager)
        return;

    for (const auto action: actionManager->actions())
        addActionMetric(action);
}

void ActionsTriggeredCountMetrics::addActionMetric(action::Action *action)
{
    const auto id = action->id();
    const auto guard = makePointer(this);
    const auto processTriggered = [this, guard, id]()
    {
        if (!guard)
            return;

        const auto kTriggerdPostfix = lit("trg");
        const QString path = makePath(kTriggerdPostfix, sender());

        auto& countByParams = m_values[id];
        ++countByParams[kTriggerdPostfix];  // Counts base trigger event number
        if (path != kTriggerdPostfix)
            ++countByParams[path];
    };

    connect(action, &QAction::triggered, this, processTriggered);
}


QnStatisticValuesHash ActionsTriggeredCountMetrics::values() const
{
    QnStatisticValuesHash result;
    for (auto it = m_values.cbegin(); it != m_values.end(); ++it)
    {
        const auto actionId = static_cast<action::IDType>(it.key());
        const auto alias = aliasByActionId(actionId);

        const auto &countByParams = it.value();
        for (auto itParams = countByParams.cbegin();
            itParams != countByParams.cend(); ++itParams)
        {
            const auto postfix = itParams.key();
            const auto triggeredCount = itParams.value();
            const auto finalAlias = makeAlias(alias, postfix);
            result.insert(finalAlias, QString::number(triggeredCount));
        }
    }
    return result;
}

void ActionsTriggeredCountMetrics::reset()
{
    m_values.clear();
}

//

ActionCheckedTimeMetric::ActionCheckedTimeMetric(const action::ManagerPtr& actionManager)
    : base_type(actionManager)
    , m_metrics(new QnMetricsContainer())
{
    if (!actionManager)
        return;

    for (const auto action: actionManager->actions())
        addActionMetric(action);
}

ActionCheckedTimeMetric::~ActionCheckedTimeMetric()
{}

void ActionCheckedTimeMetric::addActionMetric(action::Action *action)
{
    if (!action)
        return;

    static const auto kCheckedTimePostfix = lit("chkd_ms");

    const auto alias = aliasByActionId(action->id());
    const auto finalAlias = makeAlias(alias, kCheckedTimePostfix);
    m_metrics->addMetric(finalAlias, QnAbstractMetricPtr(
        new ActionDurationMetric(action)));
}

QnStatisticValuesHash ActionCheckedTimeMetric::values() const
{
    return m_metrics->values();
}

void ActionCheckedTimeMetric::reset()
{
    m_metrics->reset();
}
