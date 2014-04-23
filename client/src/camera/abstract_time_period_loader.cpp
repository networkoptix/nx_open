#include "abstract_time_period_loader.h"

QnAbstractTimePeriodLoader::QnAbstractTimePeriodLoader(const QnResourcePtr &resource,  Qn::TimePeriodContent periodsType, QObject *parent): 
    QObject(parent),
    m_resource(resource),
    m_periodsType(periodsType)
{
    connect(this, &QnAbstractTimePeriodLoader::delayedReady, this, &QnAbstractTimePeriodLoader::ready, Qt::QueuedConnection);
}
