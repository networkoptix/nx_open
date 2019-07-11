#include "ref_countable_registry.h"

#include <string>
#include <memory>
#include <unordered_set>

#define NX_DEBUG_ENABLE_OUTPUT (m_verbose)
#define NX_PRINT_PREFIX (m_printPrefix)
#include <nx/kit/debug.h>

#include <nx/kit/utils.h>

namespace nx::vms::server::sdk_support {

using namespace nx::sdk;

// TODO: #mshevchenko: Consider .ini option to switch logging to VMS logs or to stderr; the same
// option may switch NX_KIT_ASSERT to NX_ASSERT.

std::string RefCountableRegistry::readableRef(const IRefCountable* refCountable, int refCount)
{
    NX_KIT_ASSERT(refCountable);
    // TODO: #mshevchenko Consider demangling via boost - requires moving impl to the Server.
    return std::string(typeid(*refCountable).name())
        + "{refCount: " + nx::kit::utils::toString(refCount) + "}";
}

void RefCountableRegistry::notifyCreated(
    const IRefCountable* const refCountable, const int refCount)
{
    NX_KIT_ASSERT(refCountable);
    NX_OUTPUT << __func__ << "(): " << readableRef(refCountable, refCount);

    NX_KIT_ASSERT(refCount == 1,
        "Created an object with refCount != 1: " + readableRef(refCountable, refCount));

    NX_KIT_ASSERT(m_refCountables.count(refCountable) == 0,
        "Created an object which is already registered: "
            + readableRef(refCountable, refCount));

    m_refCountables.insert(refCountable);
}

void RefCountableRegistry::notifyDestroyed(
    const IRefCountable* const refCountable, const int refCount)
{
    NX_KIT_ASSERT(refCountable);
    NX_OUTPUT << __func__ << "(): " << readableRef(refCountable, refCount);

    NX_KIT_ASSERT(refCount == 0,
        "Destroying an object with refCount != 0: " + readableRef(refCountable, refCount));

    if (!NX_KIT_ASSERT(m_refCountables.count(refCountable) != 0,
        "Destroying an object which has not been registered: "
            + readableRef(refCountable, refCount)))
    {
        return;
    }

    if (m_refCountables.erase(refCountable) != 1)
    {
        NX_PRINT << "INTERNAL ERROR: Unable to unregister the object being destroyed: "
            << readableRef(refCountable, refCount);
        NX_KIT_ASSERT(false);
    }
}

RefCountableRegistry::~RefCountableRegistry()
{
    if (m_refCountables.empty())
    {
        NX_OUTPUT << __func__ << "(): SUCCESS: No registered objects remain.";
        return;
    }

    std::string message = m_printPrefix + "The following objects remain registered:";
    for (const auto& refCountable: m_refCountables)
        message += "\n" + readableRef(refCountable, refCountable->refCountThreadUnsafe());

    // Always fails; the condition is added for better output.
    NX_KIT_ASSERT(m_refCountables.empty(), message);
}

} // namespace nx::vms::server::sdk_support
