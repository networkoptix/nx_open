// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_ref_countable.h>

namespace nx::sdk {

/**
 * Debugging mechanism that tracks ref-countable objects to detect leaks, double-frees and the
 * like. This registry works automatically for any classes inherited from nx::sdk::RefCountable.
 *
 * An assertion will fail if any discrepancy is detected.
 *
 * The Server and each Plugin have their own instance of such registry, created and assigned to
 * each LibContext by the Server. Thus, each registry is tracking objects created/destroyed in the
 * respective library.
 *
 * The instance of a registry is destroyed when the LibContext destructor is called, i.e. on the
 * static deinitialization phase. On destruction, it attempts to detect and log objects which were
 * not deleted, failing an assertion if there are any such objects.
 *
 * For a high-level description of this mechanism, see @ref md_src_nx_sdk_helpers_lib_context.
 */
class IRefCountableRegistry
{
public:
    /** Logs the remaining (leaked) ref-countable objects. */
    virtual ~IRefCountableRegistry() = default;

    virtual void notifyCreated(
        const nx::sdk::IRefCountable* refCountable, int refCount) = 0;

    virtual void notifyDestroyed(
        const nx::sdk::IRefCountable* refCountable, int refCount) = 0;
};

} // namespace nx::sdk
