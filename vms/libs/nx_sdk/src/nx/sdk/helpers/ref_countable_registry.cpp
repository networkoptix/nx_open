#include "ref_countable_registry.h"

#include <string>
#include <memory>
#include <unordered_set>

#define NX_DEBUG_ENABLE_OUTPUT (m_verbose)
#define NX_PRINT_PREFIX (m_printPrefix)
#include <nx/kit/debug.h>

#include <nx/kit/utils.h>

namespace nx {
namespace sdk {

namespace {

class RefCountableRegistryImpl
{
public:
    ~RefCountableRegistryImpl();

    static void createInstance(const char* name, bool verbose)
    {
        // TODO: #mshevchenko: Find a better solution to use NX_KIT_ASSERT() in static methods.
        #undef NX_PRINT_PREFIX
        #define NX_PRINT_PREFIX "[" __FILE__ "]"
        if (!NX_KIT_ASSERT(!instanceHolder())) //< Prohibit double-initialization.
            return;
        #undef NX_PRINT_PREFIX
        #define NX_PRINT_PREFIX (m_printPrefix)

        instanceHolder().reset(new RefCountableRegistryImpl(name, verbose));
    }

    static RefCountableRegistryImpl* instance() { return instanceHolder().get(); }

    void notifyCreated(const IRefCountable* refCountable, int refCount);
    void notifyDestroyed(const IRefCountable* refCountable, int refCount);

private:
    RefCountableRegistryImpl(const char* name, bool verbose):
        m_name(name),
        m_verbose(verbose),
        m_printPrefix("RefCountableRegistry{" + m_name + "}: ")
    {
    }

    static std::unique_ptr<RefCountableRegistryImpl>& instanceHolder()
    {
        static std::unique_ptr<RefCountableRegistryImpl> instance;
        return instance;
    }

    std::string readableRef(const IRefCountable* refCountable, int refCount)
    {
        NX_KIT_ASSERT(refCountable);
        return std::string(typeid(*refCountable).name())
            + "{refCount: " + nx::kit::utils::toString(refCount) + "}";
    }

private:
    const std::string m_name;
    const bool m_verbose;
    const std::string m_printPrefix;
    std::unordered_set<const IRefCountable*> m_refCountables;
};

void RefCountableRegistryImpl::notifyCreated(
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

void RefCountableRegistryImpl::notifyDestroyed(
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

RefCountableRegistryImpl::~RefCountableRegistryImpl()
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

} // namespace

/*static*/ void RefCountableRegistry::notifyCreated(
    const IRefCountable* refCountable, int refCount)
{
    if (RefCountableRegistryImpl::instance())
        RefCountableRegistryImpl::instance()->notifyCreated(refCountable ,refCount);
}

/*static*/ void RefCountableRegistry::notifyDestroyed(
    const IRefCountable* refCountable, int refCount)
{
    if (RefCountableRegistryImpl::instance())
        RefCountableRegistryImpl::instance()->notifyDestroyed(refCountable ,refCount);
}

} // namespace sdk
} // namespace nx

void createNxRefCountableRegistry(const char* name, bool verbose)
{
    nx::sdk::RefCountableRegistryImpl::createInstance(name, verbose);
}
