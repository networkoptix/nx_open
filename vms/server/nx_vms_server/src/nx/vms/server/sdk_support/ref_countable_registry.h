#pragma once

#include <mutex>
#include <string>
#include <mutex>
#include <unordered_set>

#include <nx/sdk/helpers/i_ref_countable_registry.h>

namespace nx::vms::server::sdk_support {

/**
 * Implementation of all instances of nx::sdk::IRefCountableRegistry.
 *
 * These registries must be enabled by vms_server_plugins.ini; there are also options for logging.
 */
class RefCountableRegistry: public nx::sdk::IRefCountableRegistry
{
public:
    /** @return New instance of the registry, or null if the registry is disabled by .ini. */
    static RefCountableRegistry* createIfEnabled(const std::string& name);

    virtual ~RefCountableRegistry() override;

    virtual void notifyCreated(
        const nx::sdk::IRefCountable* refCountable, int refCount) override;

    virtual void notifyDestroyed(
        const nx::sdk::IRefCountable* refCountable, int refCount) override;

private:
    RefCountableRegistry(const std::string& name);

    std::string readableRef(const nx::sdk::IRefCountable* refCountable, int refCount) const;

private:
    std::mutex m_mutex;
    const std::string m_logPrefix;
    const bool m_useServerLog;
    const bool m_isVerbose;
    std::unordered_set<const nx::sdk::IRefCountable*> m_refCountables;
};

} // namespace nx::vms::server::sdk_support
