// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: When Storage and Camera SDKs are merged into Analytics SDK, rename this file and keep
// only Setting, Plugin and Plugin2 (renamed as `deprecated`) to be used when loading old plugins.

//!VMS dynamic plugin API (c++)
/*!
    - inspired by COM
    - each plugin MUST export function of type \a nxpl::CreateNXPluginInstanceProc with name \a createNXPluginInstance with "C" linkage
    - each interface MUST inherit \a nxpl::PluginInterface
    - each interface has GUID (\a IID_{interface_name} const non-member of type \a nxpl::NX_GUID)

    Reference counting rules:\n
    - if method returns pointer to interface derived from nxpl::PluginInterface then nxpl::PluginInterface::addRef is called by that method
        and nxpl::PluginInterface::releaseRef MUST be called by user to free up resources

    \note Use in multithreaded environment:\n
        - \a PluginInterface::releaseRef is not guaranteed to be called from thread that called \a PluginInterface::addRef,
            so reference counting - related functionality MUST be thread-safe
*/
namespace nxpl
{
    //!GUID of plugin interface
    struct NX_GUID
    {
        //!GUID bytes
        unsigned char bytes[16];
    };

    // {E03B8532-9531-41d6-982A-CA7BF0269780}
    static const nxpl::NX_GUID IID_PluginInterface =
        {{0xe0,0x3b,0x85,0x32,0x95,0x31,0x41,0xd6,0x98,0x2a,0xca,0x7b,0xf0,0x26,0x97,0x80}};

    //!Base class for every interface, provided by plugin
    /*!
        Responsible for object life-time tracking and up-cast

        Life-time tracking is done by using reference counter which is altered by \a addRef and \a releaseRef methods
        Every object has reference count of 1 just after creation.
        When reference counter reaches zero, object MUST remove itself
    */
    class PluginInterface
    {
    public:
        virtual ~PluginInterface() {}

        //!Cast to type, specified by \a interfaceID
        /*!
            If pointer cannot be cast, NULL MUST be returned
            \return If not NULL, returned pointer can be safely cast to type, defined by \a interfaceID
            \note This method increments reference counter
        */
        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) = 0;
        //!Increment reference counter
        /*!
            \return new reference count
            \note \a PluginInterface::releaseRef is not guaranteed to be called from thread that called \a PluginInterface::addRef
        */
        virtual int addRef() const = 0;
        //!Decrement reference counter
        /*!
            When zero, object MUST be removed
            \return new reference count
            \note \a PluginInterface::releaseRef is not guaranteed to be called from thread that called \a PluginInterface::addRef
        */
        virtual int releaseRef() const = 0;
    };

    struct Setting
    {
        /**
         * 0-terminated setting name.
         */
        const char* name;

        /** 0-terminated setting value. */
        const char* value;

        Setting(): name(nullptr), value(nullptr) {}
        Setting(const char* name, const char* value): name(name), value(value) {}
    };

    static const nxpl::NX_GUID IID_Plugin =
        {{0xe5,0x3c,0xf9,0x3d,0x61,0xd3,0x42,0x61,0x9d,0x25,0x9b,0x7b,0x3f,0x3a,0x81,0x2b}};

    /**
     * Optional interface with general plugin features (name, settings). Server tries to cast
     * the initial pointer received from createNXPluginInstance() to this type right after the
     * plugin is loaded.
     */
    class Plugin: public nxpl::PluginInterface
    {
    public:
        static constexpr const char* kEntryPointFuncName = "createNXPluginInstance";

        /** Prototype of a plugin entry point function. */
        typedef PluginInterface* (*EntryPointFunc)();

        /** Name of the plugin, used for information purpose only. */
        virtual const char* name() const = 0;

        /**
         * Used by the server to assign settings to the plugin. Called before other plugin
         * features are used.
         * @param settings This memory cannot be accessed after this method returns.
         * @param count Size of the settings array.
        */
        virtual void setSettings(const nxpl::Setting* settings, int count) = 0;
    };

    static const nxpl::NX_GUID IID_Plugin2 =
        {{0x10,0x0a,0xfc,0x3e,0xca,0x63,0x47,0xfb,0x9d,0x5d,0x4,0x40,0xfc,0x59,0xf8,0x66}};

    class Plugin2: public Plugin
    {
    public:
        /**
         * Provides an object which the plugin can use for calling back to access some data and
         * functionality provided by the process that uses the plugin.
         */
        virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) = 0;
    };
}
