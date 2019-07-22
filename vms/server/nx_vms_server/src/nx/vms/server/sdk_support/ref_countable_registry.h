#pragma once

#include <mutex>
#include <string>
#include <unordered_set>

#include <nx/sdk/helpers/i_ref_countable_registry.h>

namespace nx::vms::server::sdk_support {

/**
 * Debugging mechanism that tracks ref-countable objects to detect leaks and double-frees.
 *
 * The registry works automatically for any classes inherited from nx::sdk::RefCountable.
 *
 * An assertion will fail if any discrepancy is detected.
 *
 * The Server and each Plugin have their own instance of such registry, tracking objects
 * created/destroyed in the respective module. Such instances are created as global variables by
 * value, thus, are never physically destroyed. Their destructors (called on static
 * deinitialization phase) attempt to detect and log objects which were not deleted, failing an
 * assertion if there were any such objects.
 *
 * Type of logging (Server log vs stderr) and type of assertions (NX_KIT_ASSERT vs NX_ASSERT) are
 * determined by vms_server_plugins.ini, as well as the logging verbosity.
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
