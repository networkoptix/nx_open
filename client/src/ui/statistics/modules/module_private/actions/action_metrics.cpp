
#include "action_metrics.h"

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>

namespace
{
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
        if (!className.isEmpty())
            tagsList.append(className);

        return tagsList.join(L'_');
    }
}

const QString ActionTriggeredCountMetric::kPostfix = lit("trg");

ActionTriggeredCountMetric::ActionTriggeredCountMetric(QnActionManager *actionManager
    , QnActions::IDType id)
    : base_type()
    , AbstractActionMetric()
    , m_values()
{
    const auto action = actionManager->action(id);
    connect(action, &QAction::triggered
        , this, &ActionTriggeredCountMetric::onTriggered);
}

void ActionTriggeredCountMetric::onTriggered()
{
    const QString path = makePath(kPostfix, sender());

    ++m_values[kPostfix];  // Counts base trigger event number
    ++m_values[path];
}

QnMetricsHash ActionTriggeredCountMetric::metrics() const
{
    QnMetricsHash result;
    for (auto it = m_values.cbegin(); it != m_values.end(); ++it)
        result.insert(it.key(), QString::number(it.value()));

    return result;
}

void ActionTriggeredCountMetric::reset()
{
    m_values.clear();
}

//

ActionCheckedTimeMetric::ActionCheckedTimeMetric(QnActionManager *actionManager
    , QnActions::IDType id)
    : base_type()
    , ActiveTimeMetric()
{
    auto action = actionManager->action(id);
    connect(action, &QAction::toggled, this
        , &ActionCheckedTimeMetric::updateCounter);
}

ActionCheckedTimeMetric::~ActionCheckedTimeMetric()
{}

void ActionCheckedTimeMetric::updateCounter(bool activate)
{
    activateCounter(activate);
}

