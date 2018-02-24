#pragma once

#include <deque>
#include <map>
#include <string>

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

    /**
     * Adds new entries to existing list of entries, name is resolved to.
     */
    void addMapping(const std::string& name, std::deque<AddressEntry> entries);
    /**
     * Replaces entries name is resolved to.
     */
    void replaceMapping(const std::string& name, std::deque<AddressEntry> entries);
    void removeMapping(const std::string& name);
    void removeMapping(const std::string& name, const AddressEntry& entryToRemove);

private:
    mutable QnMutex m_mutex;
    std::map<std::string, std::deque<AddressEntry>> m_etcHosts;
};

} // namespace network
} // namespace nx
