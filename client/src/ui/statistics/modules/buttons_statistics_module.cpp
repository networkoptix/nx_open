
#include "buttons_statistics_module.h"

namespace
{
    const auto kMetricPrefix = lit("btn_clck_cnt");
}

QnButtonsStatisticsModule::QnButtonsStatisticsModule()
    : QnAbstractStatisticsModule()
    , m_clicksCount()
{
}

QnButtonsStatisticsModule::~QnButtonsStatisticsModule()
{
}

QnStatisticValuesHash QnButtonsStatisticsModule::values() const
{
    QnStatisticValuesHash result;
    for (auto it = m_clicksCount.cbegin(); it != m_clicksCount.cend(); ++it)
    {
        const auto alias = it.key();
        const auto fullAlias = lit("%1_%2").arg(kMetricPrefix, alias);
        const auto value = QString::number(it.value());
        result.insert(alias, value);
    }
    return result;
}

void QnButtonsStatisticsModule::reset()
{
    m_clicksCount.clear();
}

void QnButtonsStatisticsModule::onButtonPressed(const QString &alias)
{
    qDebug() << "Button clicked: " << alias;
    ++m_clicksCount[alias];
}
