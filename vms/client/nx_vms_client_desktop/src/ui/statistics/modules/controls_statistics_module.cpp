// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "controls_statistics_module.h"

namespace
{
    const QString kMetricPrefix("clck_cnt");
}

QnControlsStatisticsModule::QnControlsStatisticsModule(QObject* parent) :
    QnAbstractStatisticsModule(parent),
    m_clicksCount()
{}

QnControlsStatisticsModule::~QnControlsStatisticsModule()
{}

QnStatisticValuesHash QnControlsStatisticsModule::values() const
{
    QnStatisticValuesHash result;
    for (auto it = m_clicksCount.cbegin(); it != m_clicksCount.cend(); ++it)
    {
        const auto alias = it.key();
        const QString fullAlias = QString("%1_%2").arg(kMetricPrefix, alias);
        const auto value = QString::number(it.value());
        result.insert(fullAlias, value);
    }
    return result;
}

void QnControlsStatisticsModule::reset()
{
    m_clicksCount.clear();
}

void QnControlsStatisticsModule::registerClick(const QString& alias)
{
    ++m_clicksCount[alias];
}
