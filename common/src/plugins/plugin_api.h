#ifndef NX_PLUGIN_API_H
#define NX_PLUGIN_API_H


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
    static const NX_GUID IID_PluginInterface = { { 0xe0, 0x3b, 0x85, 0x32, 0x95, 0x31, 0x41, 0xd6, 0x98, 0x2a, 0xca, 0x7b, 0xf0, 0x26, 0x97, 0x80 } };

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
        virtual unsigned int addRef() = 0;
        //!Decrement reference counter
        /*!
            When zero, object MUST be removed
            \return new reference count
            \note \a PluginInterface::releaseRef is not guaranteed to be called from thread that called \a PluginInterface::addRef
        */
        virtual unsigned int releaseRef() = 0;
    };

    //!Setting
    struct Setting
    {
        //!\0-terminated setting name
        /*!
            Param groups are separated using /. Example of valid names: param1, group1/param1, group1/param2
            \warning Settings, defined by plugin, MUST be prefixed with plugin_name/. Plugin name is returned by \a Plugin::name()
        */
        char* name;
        //!\0-terminated setting value
        char* value;

        Setting()
        :
            name( nullptr ),
            value( nullptr )
        {
        }
    };

    // {E53CF93D-61D3-4261-9D25-9B7B3F3A812B}
    static const NX_GUID IID_Plugin = { { 0xe5, 0x3c, 0xf9, 0x3d, 0x61, 0xd3, 0x42, 0x61, 0x9d, 0x25, 0x9b, 0x7b, 0x3f, 0x3a, 0x81, 0x2b } };

    //!Optional interface with general plugin functions (plugin name, plugin settings)
    /*!
        Server tries to cast initial pointer received from \a createNXPluginInstance to this type just after plugin load
    */
    class Plugin
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~Plugin() {}
    
        //!Name of plugin
        /*!
            This name is used for information purpose only
        */
        virtual const char* name() const = 0;
        //!Used by server to report settings to plugin
        /*!
            Called before plugin functionality is used
            \param settings This memory cannot be relied on after method returns
            \param count Size of \a settings array
            \warning Settings, defined by plugin, MUST be prefixed with plugin_name/. Plugin name is returned by \a Plugin::name()
        */
        virtual void setSettings( const nxpl::Setting* settings, int count ) = 0;
    };

    // {100AFC3E-CA63-47FB-9D5D-0440FC59F866}
    static const NX_GUID IID_Plugin2 = { { 0x10, 0x0a, 0xfc, 0x3e, 0xca, 0x63, 0x47, 0xfb, 0x9d, 0x5d, 0x4, 0x40, 0xfc, 0x59, 0xf8, 0x66 } };

    class Plugin2
    :
        public Plugin
    {
    public:
        //!Provides plugin container reference to plugin
        /*!
            "Plugin container" is a process that loaded plugin.
            This reference can be used to access some data and functionality of the container
        */
        virtual void setPluginContainer( nxpl::PluginInterface* pluginContainer ) = 0;
    };

    static const NX_GUID IID_Plugin3
        = {{0x73, 0x6b, 0xd7, 0xab, 0x46, 0x1c, 0x46, 0xf0, 0x91, 0x92, 0x56, 0x3b, 0x02, 0xed, 0xa1, 0x78}};

    class Plugin3
    :
        public Plugin2
    {
    public:
        //!Sets locale information to be used by the plugin.
        /*!
            \param locale is BCP47 language tag
        */
        virtual void setLocale(const char* locale) = 0;
    };

    //!Type of plugin entry-point function
    typedef PluginInterface* (*CreateNXPluginInstanceProc)();
}

#ifdef _WIN32
    #define NX_PLUGIN_API __declspec(dllexport)
#else
    #define NX_PLUGIN_API
#endif

//!Define to mark locale dependent output paramters and return values.
#define NX_LOCALE_DEPENDENT

//!Define to mark ascii only output parameters and return values.
#define NX_ASCII

#endif  //NX_PLUGIN_API_H
