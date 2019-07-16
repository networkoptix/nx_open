#include "ref_countable_registry.h"

#include <string>
#include <memory>
#include <unordered_set>

#define NX_PRINT_PREFIX (m_logPrefix + ": ")
#include <nx/kit/debug.h>

#include <nx/kit/utils.h>

#include <plugins/vms_server_plugins_ini.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/log/log.h>

namespace nx::vms::server::sdk_support {

using namespace nx::sdk;

RefCountableRegistry::RefCountableRegistry(const std::string& name):
    m_logPrefix("RefCountableRegistry{" + name + "}"),
    m_useServerLog(pluginsIni().useServerLogForRefCountableRegistry),
    m_isVerbose(pluginsIni().verboseRefCountableRegistry)
{
}

#define ASSERT(CONDITION, /*MESSAGE*/...) ( \
    m_useServerLog \
        ? NX_ASSERT(CONDITION, std::string(__VA_ARGS__)) \
        : NX_KIT_ASSERT(CONDITION, std::string(__VA_ARGS__)) \
)

#define INFO(MESSAGE) do \
{ \
    if (m_useServerLog) \
        NX_INFO(nx::utils::log::Tag(m_logPrefix), MESSAGE); \
    else \
        NX_PRINT << (MESSAGE); \
} while (0)

#define VERBOSE(MESSAGE) do \
{ \
    if (m_isVerbose) \
    { \
        if (m_useServerLog) \
            NX_INFO(nx::utils::log::Tag(m_logPrefix), (MESSAGE)); \
        else \
            NX_PRINT << (MESSAGE); \
    } \
} while (0)

/*static*/ RefCountableRegistry* RefCountableRegistry::createIfEnabled(const std::string& name)
{
    if (!pluginsIni().enableRefCountableRegistry)
        return nullptr;
    return new RefCountableRegistry(name);
}

std::string RefCountableRegistry::readableRef(
    const IRefCountable* refCountable, int refCount) const
{
    ASSERT(refCountable);

    return demangleTypeName(typeid(*refCountable).name()).toStdString()
        + "{"
        + "refCount: " + nx::kit::utils::toString(refCount)
        + (m_isVerbose ? (", @" + nx::kit::utils::toString(refCountable)) : "")
        + "}";
}

void RefCountableRegistry::notifyCreated(
    const IRefCountable* const refCountable, const int refCount)
{
    const auto lock = std::lock_guard(m_mutex);

    ASSERT(refCountable);
    VERBOSE("Created " + readableRef(refCountable, refCount));

    ASSERT(refCount == 1,
        "Created an object with refCount != 1: " + readableRef(refCountable, refCount));

    ASSERT(m_refCountables.count(refCountable) == 0,
        "Created an object which is already registered: " + readableRef(refCountable, refCount));

    m_refCountables.insert(refCountable);
}

void RefCountableRegistry::notifyDestroyed(
    const IRefCountable* const refCountable, const int refCount)
{
    const auto lock = std::lock_guard(m_mutex);

    ASSERT(refCountable);
    VERBOSE("Destroyed " + readableRef(refCountable, refCount));

    ASSERT(refCount == 0,
        "Destroying an object with refCount != 0: " + readableRef(refCountable, refCount));

    if (!ASSERT(m_refCountables.count(refCountable) != 0,
        "Destroying an object which has not been registered: "
            + readableRef(refCountable, refCount)))
    {
        return;
    }

    if (m_refCountables.erase(refCountable) != 1)
    {
        INFO("INTERNAL ERROR: Unable to unregister the object being destroyed: "
            + readableRef(refCountable, refCount));
        ASSERT(false);
    }
}

RefCountableRegistry::~RefCountableRegistry()
{
    const auto lock = std::lock_guard(m_mutex);

    const int count = (int) m_refCountables.size();
    if (count == 0)
    {
        VERBOSE("SUCCESS: No registered objects remain.");
        return;
    }

    std::string message = "\n" + m_logPrefix + ": The following "
        + (count > 1 ? (nx::kit::utils::toString(count) + " objects remain") : "object remains")
        + " registered:";

    for (const auto& refCountable: m_refCountables)
        message += "\n" + readableRef(refCountable, refCountable->refCountThreadUnsafe());

    // Always fails; the condition is added for better output.
    ASSERT(m_refCountables.empty(), message);
}

} // namespace nx::vms::server::sdk_support
