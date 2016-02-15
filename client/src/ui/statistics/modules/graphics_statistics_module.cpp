
#include "graphics_statistics_module.h"

#include <ui/workbench/workbench_context.h>

QnGraphicsStatisticsModule::QnGraphicsStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_context()
{

}

QnGraphicsStatisticsModule::~QnGraphicsStatisticsModule()
{

}

QnMetricsHash QnGraphicsStatisticsModule::metrics() const
{
    return QnMetricsHash();
}

void QnGraphicsStatisticsModule::resetMetrics()
{

}

void QnGraphicsStatisticsModule::setContext(QnWorkbenchContext *context)
{
    if (m_context == context)
        return;


    if (m_context)
        disconnect(m_context, nullptr, this, nullptr);

    m_context = context;
}

