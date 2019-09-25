#include "server_resource.h"

#include <common/common_module.h>
#include <nx/metrics/metrics_storage.h>
#include <api/common_message_processor.h>

namespace nx::vms::server {

MediaServerResource::MediaServerResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
    Qn::directConnect(
        commonModule->messageProcessor(),
        &QnCommonMessageProcessor::syncTimeChanged,
        this, &MediaServerResource::at_syncTimeChanged);
}

void MediaServerResource::at_syncTimeChanged(qint64 /*syncTime*/)
{
    QnMutexLocker lock(&m_mutex);
    m_timeChangeEvents++;
}

MediaServerResource::~MediaServerResource()
{
    directDisconnectAll();
}

qint64 MediaServerResource::getDelta(MediaServerResource::Metrics key, qint64 value)
{
    QnMutexLocker lock(&m_mutex);
    auto& counter = m_counters[key];
    qint64 result = value - counter;
    counter = value;
    return result;
}

qint64 MediaServerResource::getAndResetMetric(MediaServerResource::Metrics parameter)
{
    switch (parameter)
    {
    case Metrics::transactions:
    {
        const auto counter = commonModule()->metrics()->transactions().success() +
            commonModule()->metrics()->transactions().errors();
        return getDelta(parameter, counter);
    }
    case Metrics::timeChanged:
        return getDelta(parameter, m_timeChangeEvents);
    default:
        return 0;
    }
}

} // namespace nx::vms::server
