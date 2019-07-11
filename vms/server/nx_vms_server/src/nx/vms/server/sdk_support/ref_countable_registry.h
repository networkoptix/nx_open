#pragma once

#include <string>
#include <unordered_set>

#include <nx/sdk/helpers/i_ref_countable_registry.h>

namespace nx::vms::server::sdk_support {

/**
 * Debugging mechanism that tracks ref-countable objects to detect leaks and double-frees.
 *
 * The registry works automatically for any classes inherited from nx::sdk::RefCountable.
 *
 * An NX_KIT_ASSERT() will fail if any discrepancy is detected.
 *
 * The Server and each Plugin have their own instance of such registry, tracking objects
 * created/destroyed in the respective module. Such instances are created as global variables by
 * value, thus, are never physically destroyed. Their destructors (called on static
 * deinitialization phase) attempt to detect and log objects which were not deleted, failing an
 * NX_KIT_ASSERT() if there were any such objects.
 */
class RefCountableRegistry: public nx::sdk::IRefCountableRegistry
{
public:
    RefCountableRegistry(std::string name, bool verbose):
        m_name(std::move(name)),
        m_verbose(verbose),
        m_printPrefix("RefCountableRegistry{" + m_name + "}: ")
    {
    }

    virtual ~RefCountableRegistry() override;

    virtual void notifyCreated(
        const nx::sdk::IRefCountable* refCountable, int refCount) override;

    virtual void notifyDestroyed(
        const nx::sdk::IRefCountable* refCountable, int refCount) override;

private:
    std::string readableRef(const nx::sdk::IRefCountable* refCountable, int refCount);

private:
    const std::string m_name;
    const bool m_verbose;
    const std::string m_printPrefix;
    std::unordered_set<const nx::sdk::IRefCountable*> m_refCountables;
};

} // namespace nx::vms::server::sdk_support
