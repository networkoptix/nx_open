
#ifndef NX_PLUGIN_API_H
#define NX_PLUGIN_API_H


//!Network Optix dynamic plugin API
/*!
    - inspired by COM
    - each plugin MUST export function of type \a nxpl::createNXPluginInstanceProc with name \a createNXPluginInstance with "C" linkage
    - each interface MUST inherit \a nxpl::NXPluginInterface
    - each interface has GUID (\a IID_{interface_name} const non-member)

    \note Use in multithreaded environment:\n
        - \a NXPluginInterface::releaseRef is not guaranteed to be called from thread that called \a NXPluginInterface::addRef
*/
namespace nxpl
{
    struct NX_GUID
    {
        unsigned char bytes[16];
    };

    // {E03B8532-9531-41d6-982A-CA7BF0269780}
    static const NX_GUID IID_NXPluginInterface = { 0xe0, 0x3b, 0x85, 0x32, 0x95, 0x31, 0x41, 0xd6, 0x98, 0x2a, 0xca, 0x7b, 0xf0, 0x26, 0x97, 0x80 };

    //!Base class for every interface, provided by plugin
    /*!
        Responsible for object life-time tracking and up-cast

        Life-time tracking is done by using reference counter which is altered by \a addRef and \a releaseRef methods
        Every object has reference count of 1 just after creation.
        When reference counter reaches zero, object MUST remove itself
    */
    class NXPluginInterface
    {
    public:
        virtual ~NXPluginInterface() {}

        //!Cast to type, specified by \a interfaceID
        /*!
            If pointer cannot be cast, NULL MUST be returned
            \return If not NULL, returned pointer can be safely cast to type, defined by \a interfaceID
            \note This method increments reference counter
        */
        virtual void* queryInterface( const NX_GUID& interfaceID ) = 0;
        //!Increment reference counter
        /*!
            \return new reference count
            \note \a NXPluginInterface::releaseRef is not guaranteed to be called from thread that called \a NXPluginInterface::addRef
        */
        virtual unsigned int addRef() = 0;
        //!Decrement reference counter
        /*!
            When zero, object MUST be removed
            \return new reference count
            \note \a NXPluginInterface::releaseRef is not guaranteed to be called from thread that called \a NXPluginInterface::addRef
        */
        virtual unsigned int releaseRef() = 0;
    };

    //!Type of plugin entry-point function
    typedef NXPluginInterface* (*CreateNXPluginInstanceProc)();
}

#endif  //NX_PLUGIN_API_H
