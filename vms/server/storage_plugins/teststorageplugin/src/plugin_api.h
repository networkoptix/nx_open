#ifndef NX_PLUGIN_API_H
#define NX_PLUGIN_API_H


namespace nxpl
{
    struct NX_GUID
    {
        unsigned char bytes[16];
    };

    // {E03B8532-9531-41d6-982A-CA7BF0269780}
    static const NX_GUID IID_PluginInterface = { { 0xe0, 0x3b, 0x85, 0x32, 0x95, 0x31, 0x41, 0xd6, 0x98, 0x2a, 0xca, 0x7b, 0xf0, 0x26, 0x97, 0x80 } };

    class PluginInterface
    {
    public:
        virtual ~PluginInterface() {}
        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) = 0;
        virtual unsigned int addRef() = 0;
        virtual unsigned int releaseRef() = 0;
    };

    struct Setting
    {
        char* name;
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

    class Plugin
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~Plugin() {}
        virtual const char* name() const = 0;
        virtual void setSettings( const nxpl::Setting* settings, int count ) = 0;
    };

    // {100AFC3E-CA63-47FB-9D5D-0440FC59F866}
    static const NX_GUID IID_Plugin2 = { { 0x10, 0x0a, 0xfc, 0x3e, 0xca, 0x63, 0x47, 0xfb, 0x9d, 0x5d, 0x4, 0x40, 0xfc, 0x59, 0xf8, 0x66 } };

    class Plugin2
    :
        public Plugin
    {
    public:
        virtual void setPluginContainer( nxpl::PluginInterface* pluginContainer ) = 0;
    };

    typedef PluginInterface* (*CreateNXPluginInstanceProc)();
}

#endif  //NX_PLUGIN_API_H
