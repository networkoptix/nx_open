
#pragma once

#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/statistics/abstract_statistics_metric.h>

namespace statistics
{
    namespace details
    {
        enum ActionEmitterType
        {
            kMenu           = 0x01
            , kShortcut     = 0x02
            , kObject       = 0x03

            , kManual       = 0x04

            , kAllEmitters  = (kMenu | kShortcut | kObject | kManual)
        };

        struct ActionMetricParams
        {
            MetricType type;
            ActionEmitterType filter;
            QLatin1String objectTypeFilter;

            ActionMetricParams(MetricType initType = kOccuranciesCount
                , ActionEmitterType initFilter = kAllEmitters
                , QLatin1String initObjectTypeFilter = QLatin1String(nullptr));
        };

        class ActionsStatisticHandler : protected Connective<QnWorkbenchContextAware>
        {
            typedef Connective<QnWorkbenchContextAware> BaseType;

        public:
            ActionsStatisticHandler(QObject *parent);

            virtual ~ActionsStatisticHandler();

            void watchAction(int actionId
                , const QString &alias
                , const ActionMetricParams &params);

        private:
            typedef QSharedPointer<QnAbstractStatisticsMetric> QnAbstractStatisticsMetricPtr;
            typedef QList<QnAbstractStatisticsMetricPtr> MetricsList;
            MetricsList m_metrics;
        };
    }
}

Q_DECLARE_METATYPE(statistics::details::ActionEmitterType);
