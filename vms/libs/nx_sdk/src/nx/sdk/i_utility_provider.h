// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_string.h>

namespace nx {
namespace sdk {

/**
 * Represents an object which the plugin can use for calling back to access some data and
 * functionality provided by the process that uses the plugin.
 *
 * To use this object, request an object implementing a particular I...UtilityProvider via
 * queryInterface(). All such interfaces in the current SDK version are supported, but if a plugin
 * intends to support VMS versions using some older SDK, it should be ready to accept the denial.
 *
 * NOTE: Is binary-compatible with the old SDK's nxpl::TimeProvider and supports its interface id.
 */
class IUtilityProvider0: public Interface<IUtilityProvider0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IUtilityProvider"); }

    /**
     * VMT #4.
     * @return Synchronized System time - common time for all Servers in the VMS System.
     */
    virtual int64_t vmsSystemTimeSinceEpochMs() const = 0;

    /** Called by homeDir() */
    protected: virtual const IString* getHomeDir() const = 0;
    /**
     * The dynamic library of a Plugin can either reside in the directory designated for all
     * plugins, together with other plugins, or in its subdirectory containing the dynamic library
     * and potentially other files (which are ignored by the Server), e.g. other dynamic libraries
     * the Plugin depends on, or some resource or configuration files that are loaded at runtime.
     * If a Plugin resides in such dedicated subdirectory, it is called Plugin's Home Directory.
     * Its name must be equal to the Plugin's libName - the name of the Plugin dynamic library
     * without the `lib` prefix (on Linux) and the extension.
     *
     * @return Absolute path to the Plugin's Home Directory, or an empty string if it is absent.
     */
    public: std::string homeDir() const { return toPtr(getHomeDir())->str(); }
};

class IUtilityProvider1: public Interface<IUtilityProvider1, IUtilityProvider0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IUtilityProvider1"); }

    /** Called by serverSdkVersion() */
    protected: virtual const IString* getServerSdkVersion() const = 0;
    /**
     * @return The version of the Server's built-in SDK.
     */
    public: std::string serverSdkVersion() const { return toPtr(getServerSdkVersion())->str(); }
};

class IUtilityProvider2: public Interface<IUtilityProvider2, IUtilityProvider1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IUtilityProvider2"); }

    virtual const char* serverId() const = 0;
};

class IUtilityProvider: public Interface<IUtilityProvider, IUtilityProvider2>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IUtilityProvider3"); }

    virtual IString* cloudSystemId() const = 0;
    virtual IString* cloudAuthKey() const = 0;
};
using IUtilityProvider3 = IUtilityProvider;

} // namespace sdk
} // namespace nx
