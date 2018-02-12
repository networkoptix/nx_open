#pragma once

#include <deque>
#include <map>

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

    void addMapping(const QString& name, std::deque<AddressEntry> entries);
    void removeMapping(const QString& name);
    void removeMapping(const QString& name, const AddressEntry& entryToRemove);

private:
    mutable QnMutex m_mutex;
    std::map<QString, std::deque<AddressEntry>> m_etcHosts;
};

} // namespace network
} // namespace nx
