#include "abstract_time_period_loader.h"

QnAbstractTimePeriodLoader::QnAbstractTimePeriodLoader(const QnResourcePtr &resource,  Qn::TimePeriodContent periodsType, QObject *parent): 
    QObject(parent),
    m_resource(resource),
    m_periodsType(periodsType)
{
    connect(this, SIGNAL(delayedReady(const QnTimePeriodList &, int)), this, SIGNAL(ready(const QnTimePeriodList &, int)), Qt::QueuedConnection);
}
