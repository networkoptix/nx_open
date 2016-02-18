
#include "controls_statistics_module.h"

namespace
{
    const auto kMetricPrefix = lit("clck_cnt");
}

QnControlsStatisticsModule::QnControlsStatisticsModule()
    : QnAbstractStatisticsModule()
    , m_clicksCount()
{
}

QnControlsStatisticsModule::~QnControlsStatisticsModule()
{
}

QnStatisticValuesHash QnControlsStatisticsModule::values() const
{
    QnStatisticValuesHash result;
    for (auto it = m_clicksCount.cbegin(); it != m_clicksCount.cend(); ++it)
    {
        const auto alias = it.key();
        const auto fullAlias = lit("%1_%2").arg(kMetricPrefix, alias);
        const auto value = QString::number(it.value());
        result.insert(fullAlias, value);
    }
    return result;
}

void QnControlsStatisticsModule::reset()
{
    m_clicksCount.clear();
}

void QnControlsStatisticsModule::registerPressEvent(const QString &alias)
{
    qDebug() << "Button clicked: " << alias;
    ++m_clicksCount[alias];
}
