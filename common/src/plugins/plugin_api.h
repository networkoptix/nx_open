#ifndef NX_PLUGIN_API_H
#define NX_PLUGIN_API_H


//!Network Optix dynamic plugin API
/*!
    - inspired by COM
    - each plugin MUST export function of type \a nxpl::createNXPluginInstanceProc with name \a createNXPluginInstance with "C" linkage
    - each interface MUST inherit \a nxpl::PluginInterface
    - each interface has GUID (\a IID_{interface_name} const non-member)

    \note Use in multithreaded environment:\n
        - \a PluginInterface::releaseRef is not guaranteed to be called from thread that called \a PluginInterface::addRef
*/
namespace nxpl
{
    // TODO: #Elric rename to Guid?
    struct NX_GUID
    {
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
        virtual void* queryInterface( const NX_GUID& interfaceID ) = 0;
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

    /*!
        Increments object's reference counter (\a PluginInterface::addRef) at construction, decrements at destruction (\a PluginInterface::releaseRef)
        \param T MUST inherit \a PluginInterface
        \note Class is reentrant, not thread-safe
    */
    template<class T>
    class ScopedRef
    {
    public:
        ScopedRef( T* ptr = 0 )
        :
            m_ptr( 0 )
        {
            take( ptr );
        }

        ~ScopedRef()
        {
            release();
        }

        T* get()
        {
            return m_ptr;
        }

        T* operator->()
        {
            return m_ptr;
        }

        const T* operator->() const
        {
            return m_ptr;
        }

        T* release()
        {
            if( !m_ptr )
                return 0;

            T* ptrBak = m_ptr;
            m_ptr->releaseRef();
            m_ptr = 0;
            return ptrBak;
        }

        void reset( T* ptr = 0 )
        {
            release();
            take( ptr );
        }

    private:
        T* m_ptr;

        void take( T* ptr )
        {
            m_ptr = ptr;
            if( m_ptr )
                m_ptr->addRef();
        }

        ScopedRef( const ScopedRef& );
        ScopedRef& operator=( const ScopedRef& );
    };

    //!Type of plugin entry-point function
    typedef PluginInterface* (*CreateNXPluginInstanceProc)();
}

#endif  //NX_PLUGIN_API_H
