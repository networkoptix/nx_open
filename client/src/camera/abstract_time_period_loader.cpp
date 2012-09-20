#include "abstract_time_period_loader.h"

QnAbstractTimePeriodLoader::QnAbstractTimePeriodLoader(QnResourcePtr resource, QObject *parent): QObject(parent)
{
    m_resource = resource;
    qRegisterMetaType<QnTimePeriodList>("QnTimePeriodList");
    connect(this, SIGNAL(delayedReady(const QnTimePeriodList &, int)), this, SIGNAL(ready(const QnTimePeriodList &, int)), Qt::QueuedConnection);
}
