#include "device_mock.h"

namespace nx::core::resource {

QnAbstractStreamDataProvider* DeviceMock::createLiveDataProvider()
{
    return nullptr;
}

void DeviceMock::setAnalyticsSdkEventMapping(
    std::map<nx::vms::api::EventType, QString> setAnalyticsSdkEventMapping)
{
    m_analyticsSdkEventMapping = std::move(setAnalyticsSdkEventMapping);
}

QString DeviceMock::vmsToAnalyticsEventTypeId(nx::vms::api::EventType eventType) const
{
    if (const auto it = m_analyticsSdkEventMapping.find(eventType);
        it != m_analyticsSdkEventMapping.cend())
    {
        return it->second;
    }

    return QString();
}

} // namespace nx::core::resource
