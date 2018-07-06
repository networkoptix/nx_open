#include "cloud_connect_settings.h"

namespace nx {
namespace network {
namespace cloud {

CloudConnectSettings::CloudConnectSettings(const CloudConnectSettings& right):
    forcedMediatorUrl(right.forcedMediatorUrl),
    isUdpHpDisabled(right.isUdpHpDisabled),
    isOnlyCloudProxyEnabled(right.isOnlyCloudProxyEnabled),
    useHttpConnectToListenOnRelay(right.useHttpConnectToListenOnRelay),
    m_originatingHostAddressReplacement(right.originatingHostAddressReplacement())
{
}

CloudConnectSettings& CloudConnectSettings::operator=(const CloudConnectSettings& right)
{
    if (this == &right)
        return *this;

    forcedMediatorUrl = right.forcedMediatorUrl;
    isUdpHpDisabled = right.isUdpHpDisabled;
    isOnlyCloudProxyEnabled = right.isOnlyCloudProxyEnabled;
    useHttpConnectToListenOnRelay = right.useHttpConnectToListenOnRelay;

    auto val = right.originatingHostAddressReplacement();
    QnMutexLocker lk(&m_mutex);
    m_originatingHostAddressReplacement = std::move(val);

    return *this;
}

void CloudConnectSettings::replaceOriginatingHostAddress(const QString& hostAddress)
{
    QnMutexLocker lk(&m_mutex);
    m_originatingHostAddressReplacement = hostAddress;
}

boost::optional<QString> CloudConnectSettings::originatingHostAddressReplacement() const
{
    QnMutexLocker lk(&m_mutex);
    return m_originatingHostAddressReplacement;
}

} // namespace cloud
} // namespace network
} // namespace nx
