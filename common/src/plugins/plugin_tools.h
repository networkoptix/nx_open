
#ifndef PLUGIN_TOOLS_H
#define PLUGIN_TOOLS_H


namespace nxpl_tools
{
    //!Automatic scoped pointer class which uses \a PluginInterface reference counting interface (\a PluginInterface::addRef and \a PluginInterface::releaseRef) instead of new/delete
    /*!
        Increments object's reference counter (\a PluginInterface::addRef) at construction, decrements at destruction (\a PluginInterface::releaseRef)
        \param T MUST inherit \a PluginInterface
        \note Class is reentrant, not thread-safe
    */
    template<class T>
    class ScopedRef
    {
    public:
        //!Calls \a ptr->addRef() if \a ptr is not 0
        ScopedRef( T* ptr = 0 )
        :
            m_ptr( 0 )
        {
            take( ptr );
        }

        ~ScopedRef()
        {
            reset();
        }

        //!Returns protected pointer without releasing it
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

        //!Releases protected pointer without calling \a nxpl::PluginInterface::releaseRef
        T* release()
        {
            T* ptrBak = m_ptr;
            m_ptr = 0;
            return ptrBak;
        }

        //!Calls \a nxpl::PluginInterface::releaseRef on protected pointer (if any) and takes new pointer \a ptr (calling \a nxpl::PluginInterface::addRef)
        void reset( T* ptr = 0 )
        {
            if( m_ptr )
            {
                m_ptr->releaseRef();
                m_ptr = 0;
            }

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
}

#endif  //PLUGIN_TOOLS_H
