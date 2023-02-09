// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

/**
 * Defines how the Client must interact with the user after the plugin executes some action
 * triggered by the user.
 */
class IActionResponse: public Interface<IActionResponse>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IActionResponse"); }

    /**
     * @return URL to be opened by the Client in an embedded browser, or a null or empty string. If
     *     non-empty, messageToUser() must return a null or empty string.
     */
    virtual const char* actionUrl() const = 0;

    /**
     * @return Text to be shown to the user by the Client, or a null or empty string. If non-empty,
     *     actionUrl() must return a null or empty string.
     */
    virtual const char* messageToUser() const = 0;
};
using IActionResponse0 = IActionResponse;

} // namespace nx::sdk
