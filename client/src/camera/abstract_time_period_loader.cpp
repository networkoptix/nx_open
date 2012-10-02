#include "abstract_time_period_loader.h"

#include <common/common_meta_types.h>

QnAbstractTimePeriodLoader::QnAbstractTimePeriodLoader(QnResourcePtr resource, QObject *parent): 
    QObject(parent),
    m_resource(resource)
{
    QnCommonMetaTypes::initilize();

    connect(this, SIGNAL(delayedReady(const QnTimePeriodList &, int)), this, SIGNAL(ready(const QnTimePeriodList &, int)), Qt::QueuedConnection);
}
