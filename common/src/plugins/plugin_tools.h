#ifndef PLUGIN_TOOLS_H
#define PLUGIN_TOOLS_H

#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>

#include "plugin_api.h"

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
        /** Intended to be applied to queryInterface(). */
        ScopedRef(void* ptr): ScopedRef(static_cast<T*>(ptr), /*increaseRef*/ false) {}

        //!Calls \a ptr->addRef() if \a ptr is not 0 and \a increaseRef is true
        ScopedRef( T* ptr = 0, bool increaseRef = true )
        :
            m_ptr( 0 )
        {
            m_ptr = ptr;
            if( m_ptr && increaseRef )
                m_ptr->addRef();
        }

        ScopedRef(ScopedRef<T>&& right)
        {
            m_ptr = right.release();
        }

        ScopedRef<T>& operator=(ScopedRef<T>&& right)
        {
            m_ptr = right.release();
            return (*this);
        }

        ~ScopedRef()
        {
            reset();
        }

        operator bool() const
        {
            return m_ptr;
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

    //!Alignes \a val up to the boundary \a alignment
    inline size_t alignUp( size_t val, size_t alignment )
    {
        const size_t remainder = val % alignment;
        if( remainder == 0 )
            return val;
        return val + (alignment - remainder);
    }

    //!Allocate \a size bytes of data, aligned to \a alignment boundary
    /*!
        \param mallocFunc Function with signature \a void*(size_t) which is called to allocate memory
        \note Allocated memory MUST be freed with \a freeAligned call
        \note Function is as safe as \a ::malloc is
    */
    template<class MallocFunc>
    void* mallocAligned( size_t size, size_t alignment, MallocFunc mallocFunc )
    {
        if( alignment == 0 )
            return 0;
        void* ptr = mallocFunc( size + alignment + sizeof(alignment) );
        if( !ptr )   //allocation error
            return ptr;

        void* aligned_ptr = (char*)ptr + sizeof(alignment);  //leaving place to save unalignment
        const size_t unalignment = alignment - (((size_t)aligned_ptr) % alignment);
        memcpy( (char*)ptr+unalignment, &unalignment, sizeof(unalignment) );
        return (char*)aligned_ptr + unalignment;
    }

    //!Calls above function passing \a ::malloc as third argument
    inline void* mallocAligned( size_t size, size_t alignment )
    {
        return mallocAligned<>( size, alignment, ::malloc );
    }

    //!free \a ptr allocated with a call to \a mallocAligned with \a alignment
    /*!
        \param freeFunc Function with signature \a void(void*) which is called to free allocated memory
        \note Function is as safe as \a ::free is
    */
    template<class FreeFunc>
    void freeAligned( void* ptr, FreeFunc freeFunc )
    {
        if( !ptr )
            return freeFunc( ptr );

        ptr = (char*)ptr - sizeof(size_t);
        size_t unalignment = 0;
        memcpy( &unalignment, ptr, sizeof(unalignment) );
        ptr = (char*)ptr - unalignment;

        freeFunc( ptr );
    }

    //!Calls above function passing \a ::free as second argument
    inline void freeAligned( void* ptr )
    {
        return freeAligned<>( ptr, ::free );
    }

    namespace atomic
    {
#ifdef _WIN32
        typedef volatile LONG AtomicLong;
#elif __GNUC__
        typedef volatile long AtomicLong;
#else
#error "Unsupported compiler is used"
#endif

        //!Increments \a *val, returns new value
        /*!
            \return new (incremented) value
        */
        static AtomicLong inc( AtomicLong* val )
        {
#ifdef _WIN32
            return InterlockedIncrement( val );
#elif __GNUC__
            return __sync_add_and_fetch( val, 1 );
#endif
        }

        //!Decrements \a *val, returns new value
        /*!
            \return new (decremented) value
        */
        static AtomicLong dec( AtomicLong* val )
        {
#ifdef _WIN32
            return InterlockedDecrement( val );
#elif __GNUC__
            return __sync_sub_and_fetch( val, 1 );
#endif
        }
    }

    //!Implements \a nxpl::PluginInterface reference counting. Can delegate reference counting to another \a CommonRefManager instance
    /*!
        This class does not inherit nxpl::PluginInterface because it would require virtual inheritance.
        This class instance is supposed to be nested into monitored class
    */
    class CommonRefManager
    {
    public:
        //!Use this constructor delete \a objToWatch when reference counter drops to zero
        /*!
            \note Initially, reference counter is 1
        */
        CommonRefManager( nxpl::PluginInterface* objToWatch )
        :
            m_refCount( 1 ),
            m_objToWatch( objToWatch ),
            m_refCountingDelegate( 0 )
        {
        }

        //!Use this constructor to delegate reference counting to another object
        /*!
            \note Does not increment \a refCountingDelegate reference counter
        */
        CommonRefManager( CommonRefManager* refCountingDelegate )
        :
            m_refCountingDelegate( refCountingDelegate )
        {
        }

        //!Implementaion of nxpl::PluginInterface::addRef
        unsigned int addRef()
        {
            return m_refCountingDelegate
                ? m_refCountingDelegate->addRef()
                : atomic::inc(&m_refCount);
        }

        //!Implementaion of nxpl::PluginInterface::releaseRef
        /*!
            Deletes monitored object if reference counter reached zero
        */
        unsigned int releaseRef()
        {
            if( m_refCountingDelegate )
                return m_refCountingDelegate->releaseRef();

            unsigned int newRefCounter = atomic::dec(&m_refCount);
            if( newRefCounter == 0 )
                delete m_objToWatch;
            return newRefCounter;
        }

    private:
        atomic::AtomicLong m_refCount;
        nxpl::PluginInterface* m_objToWatch;
        CommonRefManager* m_refCountingDelegate;
    };


    template <typename T>
    class CommonRefCounter: public T
    {
    public:
        virtual unsigned int addRef() override
        {
            return m_refManager.addRef();
        }

        virtual unsigned int releaseRef() override
        {
            return m_refManager.releaseRef();
        }

    protected:
        nxpt::CommonRefManager m_refManager;

        CommonRefCounter():
            m_refManager(static_cast<T*>(this))
        {
        }

        CommonRefCounter(nxpt::CommonRefManager* refManager):
            m_refManager(refManager)
        {
        }
    };

    enum NxGuidFormatOption
    {
        uppercase = 0x1,
        hyphens = 0x2,
        braces = 0x4,

        applyAll = uppercase | hyphens | braces
    };

    class NxGuidHelper
    {
    public:
        static nxpl::NX_GUID fromRawData(const char* data)
        {
            nxpl::NX_GUID result;
            memcpy(result.bytes, data, sizeof(result.bytes));
            return result;
        }

        static std::string toStdString(
            const nxpl::NX_GUID& guid,
            unsigned int format = NxGuidFormatOption::applyAll)
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');

            if (format & NxGuidFormatOption::braces)
                ss << '{';

            if (format & NxGuidFormatOption::uppercase)
                ss << std::uppercase;

            for (int i = 0; i < 4; ++i)
            {
                ss << std::setw(2);
                ss << static_cast<unsigned int>(guid.bytes[i]);
            }

            if (format & NxGuidFormatOption::hyphens)
                ss << '-';

            for (int i = 0; i < 2; ++i)
            {
                ss << std::setw(2);
                ss << static_cast<unsigned int>(guid.bytes[4 + i]);
            }

            if (format & NxGuidFormatOption::hyphens)
                ss << "-";

            for (int i = 0; i < 2; ++i)
            {
                ss << std::setw(2);
                ss << static_cast<unsigned int>(guid.bytes[6 + i]);
            }

            if (format & NxGuidFormatOption::hyphens)
                ss << "-";

            for (int i = 0; i < 2; ++i)
            {
                ss << std::setw(2);
                ss << static_cast<unsigned int>(guid.bytes[8 + i]);
            }

            if (format & NxGuidFormatOption::hyphens)
                ss << "-";

            for (int i = 0; i < 6; ++i)
            {
                ss << std::setw(2);
                ss << static_cast<unsigned int>(guid.bytes[10 + i]);
            }

            if (format & NxGuidFormatOption::braces)
                ss << '}';

            return ss.str();
        }
    };



} // namespace nxpt

namespace nxpl {

inline bool operator == (const nxpl::NX_GUID& id1, const nxpl::NX_GUID& id2)
{
    return memcmp(id1.bytes, id2.bytes, sizeof(id1.bytes)) == 0;
}

} // namespace nxpl

namespace std {

template<>
struct hash<nxpl::NX_GUID>
{
    std::size_t operator()(const nxpl::NX_GUID& guid) const
    {
        std::size_t h;

        for (auto i = 0; i < sizeof(guid.bytes); ++i)
            h = (h + (324723947 + guid.bytes[i])) ^ 93485734985;

        return h;
    }
};

} // namespace std

#endif  //PLUGIN_TOOLS_H
