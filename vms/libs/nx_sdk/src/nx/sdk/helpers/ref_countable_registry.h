#pragma once

#include <nx/sdk/i_ref_countable.h>

namespace nx {
namespace sdk {

// TODO: #mshevchenko: Revise the comment, note `instance` type and lifetime.
/**
 * Debugging mechanism that tracks ref-countable objects to detect leaks and double-frees.
 *
 * An NX_KIT_ASSERT() will fail if a discrepancy is detected.
 *
 * The Server and each Plugin have their own instance of such registry, tracking objects
 * created/destroyed in the respective module. Such instances are created as global variables by
 * value, thus, are never physically destroyed. Their destructors (called on static
 * deinitialization phase) attempt to detect and log objects which were not deleted.
 */
class RefCountableRegistry
{
public: //< Intended to be used by RefCountable when creating/destroying objects.
    static void notifyCreated(const IRefCountable* refCountable, int refCount);
    static void notifyDestroyed(const IRefCountable* refCountable, int refCount);

public: //< Intended to be used by VMS Server when loading a Plugin.
    RefCountableRegistry() = delete;

    /** Name of a Plugin's exported function that creates RefCountableRegistry instance. */
    static constexpr const char kCreateNxRefCountableRegistryFuncName[] =
        "createNxRefCountableRegistry";

    /** Prototype a Plugin's exported function that creates RefCountableRegistry instance. */
    typedef void (*CreateNxRefCountableRegistryFunc)(const char* name, bool verbose);
};

} // namespace sdk
} // namespace nx

extern "C" NX_SDK_API void createNxRefCountableRegistry(const char* name, bool verbose);
