// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_error.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin.h>

#include "i_engine.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * The main interface for an Analytics Plugin instance.
 */
class IPlugin: public Interface<IPlugin, nx::sdk::IPlugin>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IPlugin"); }

    /**
     * Provides plugin manifest in JSON format.
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure.
     * @return JSON string in UTF-8.
     */
    virtual const IString* manifest(IError* outError) const = 0;

    /**
     * Creates a new instance of Analytics Engine.
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure.
     * @return Pointer to an object that implements DeviceAgent interface, or null in case of
     *     failure.
     */
    virtual IEngine* createEngine(IError* outError) = 0;

    /**
     * Name of the plugin dynamic library, without "lib" prefix and without extension.
     */
    virtual const char* name() const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
