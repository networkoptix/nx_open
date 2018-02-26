#pragma once

#include <deque>
#include <map>
#include <string>

#include <nx/utils/thread/mutex.h>

#include "abstract_resolver.h"

namespace nx {
namespace network {

/**
 * Resolves name to a manually-added address entry(-ies).
 * NOTE: Unknown low-level domain name resolves to a higher-level domain name.
 * E.g.,
 * <pre><code>
 *   addMapping("x.y", {xy_entry});
 *   resolve("x.y");    // Resolved to xy_entry.
 *   resolve("y");      // Resolved to xy_entry.
 *   resolve("x");      // Resolve failed.
 *   addMapping("y", {y_entry});
 *   resolve("y");      // Resolved to y_entry.
 * </code></pre>
 */
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
    /**
     * Name to address list.
     * Name is stored in a reversed order. E.g, "something.example.com" will be stored as
     * "com.example.something". It makes resolve of "example.com" more convenient.
     */
    std::map<std::string, std::deque<AddressEntry>> m_etcHosts;
};

} // namespace network
} // namespace nx
