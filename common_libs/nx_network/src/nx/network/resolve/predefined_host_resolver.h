#pragma once

#include <map>
#include <vector>

#include <nx/utils/thread/mutex.h>

#include "abstract_resolver.h"

namespace nx {
namespace network {

class NX_NETWORK_API PredefinedHostResolver:
    public AbstractResolver
{
public:
    virtual SystemError::ErrorCode resolve(
        const QString& name,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses) override;

    void addEtcHost(const QString& name, std::vector<AddressEntry> entries);
    void removeEtcHost(const QString& name);

private:
    mutable QnMutex m_mutex;
    std::map<QString, std::vector<AddressEntry>> m_etcHosts;
};

} // namespace network
} // namespace nx
