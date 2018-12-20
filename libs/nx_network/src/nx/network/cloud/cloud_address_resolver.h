#pragma once

#include <QtCore/QRegExp>

#include <nx/utils/thread/mutex.h>

#include "../resolve/abstract_resolver.h"

namespace nx {
namespace network {

class CloudAddressResolver:
    public AbstractResolver
{
public:
    CloudAddressResolver();

    virtual SystemError::ErrorCode resolve(
        const QString& hostName,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) override;

private:
    const QRegExp m_cloudAddressRegExp;
    QnMutex m_mutex;
};

} // namespace network
} // namespace nx
