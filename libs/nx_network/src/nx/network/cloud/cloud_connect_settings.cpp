// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_connect_settings.h"

namespace nx::network::cloud {

CloudConnectSettings::CloudConnectSettings(const CloudConnectSettings& right):
    forcedMediatorUrl(right.forcedMediatorUrl),
    isUdpHpEnabled(right.isUdpHpEnabled),
    isCloudProxyEnabled(right.isCloudProxyEnabled),
    isDirectTcpConnectEnabled(right.isDirectTcpConnectEnabled),
    delayBeforeSendingConnectToMediatorOverTcp(right.delayBeforeSendingConnectToMediatorOverTcp),
    cloudConnectTimeout(right.cloudConnectTimeout),
    m_originatingHostAddressReplacement(right.originatingHostAddressReplacement())
{
}

CloudConnectSettings& CloudConnectSettings::operator=(const CloudConnectSettings& right)
{
    if (this == &right)
        return *this;

    forcedMediatorUrl = right.forcedMediatorUrl;
    isUdpHpEnabled = right.isUdpHpEnabled;
    isCloudProxyEnabled = right.isCloudProxyEnabled;
    isDirectTcpConnectEnabled = right.isDirectTcpConnectEnabled;
    delayBeforeSendingConnectToMediatorOverTcp = right.delayBeforeSendingConnectToMediatorOverTcp;
    cloudConnectTimeout = right.cloudConnectTimeout;

    auto val = right.originatingHostAddressReplacement();
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_originatingHostAddressReplacement = std::move(val);

    return *this;
}

void CloudConnectSettings::replaceOriginatingHostAddress(const std::string& hostAddress)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_originatingHostAddressReplacement = hostAddress;
}

std::optional<std::string> CloudConnectSettings::originatingHostAddressReplacement() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_originatingHostAddressReplacement;
}

} // namespace nx::network::cloud
