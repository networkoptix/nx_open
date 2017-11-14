#include "cloud_connect_settings.h"

namespace nx {
namespace network {
namespace cloud {

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
