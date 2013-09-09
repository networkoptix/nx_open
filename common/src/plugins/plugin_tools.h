
#ifndef PLUGIN_TOOLS_H
#define PLUGIN_TOOLS_H

#include <cstdlib>
#include <cstring>


namespace nxpt
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
        //!Calls \a ptr->addRef() if \a ptr is not 0 and \a increaseRef is true
        ScopedRef( T* ptr = 0, bool increaseRef = true )
        :
            m_ptr( 0 )
        {
            m_ptr = ptr;
            if( m_ptr && increaseRef )
                m_ptr->addRef();
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

    /*!
        Using this macro for systems with CHAR_BITS != 8, since sizeof returnes size in number of chars, but memcpy accepts bytes
    */
#define SIZEOF_IN_BYTES( type ) ((size_t)(((type*)0)+1) - (size_t)((type*)0))

    //!Allocate \a size bytes of data, aligned to \a alignment boundary
    /*!
        \note Allocated memory MUST be freed with \a freeAligned call
        \note Function is as safe as \a ::malloc is
    */
    static void* mallocAligned( size_t size, size_t alignment )
    {
        if( alignment == 0 )
            return 0;
        void* ptr = ::malloc( size + alignment + SIZEOF_IN_BYTES(size_t) );
        if( !ptr )   //allocation error
            return ptr;

        void* aligned_ptr = (void*)((size_t)ptr + SIZEOF_IN_BYTES(size_t));  //leaving place to save unalignment
        const size_t unalignment = alignment - ((size_t)aligned_ptr) % alignment;
        memcpy( (void*)((size_t)ptr+unalignment), &unalignment, SIZEOF_IN_BYTES(size_t) );
        return (void*)((size_t)aligned_ptr + unalignment);
    }

    //!free \a ptr allocated with a call to \a mallocAligned with \a alignment
    /*!
        \note Function is as safe as \a ::free is
    */
    static void freeAligned( void* ptr )
    {
        if( !ptr )
            return ::free( ptr );

        ptr = (void*)((size_t)ptr - SIZEOF_IN_BYTES(size_t));
        size_t unalignment = 0;
        memcpy( &unalignment, ptr, SIZEOF_IN_BYTES(size_t) );
        ptr = (void*)((size_t)ptr -  unalignment);

        ::free( ptr );
    }
}

#endif  //PLUGIN_TOOLS_H
