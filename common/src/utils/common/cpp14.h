/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_cpp14_h
#define libcommon_cpp14_h

#include <atomic>
#include <memory>

#ifdef __GNUC__
#include <features.h>
#endif


////////////////////////////////////////////////////////////////////////////
// Some c++14 features missing in MS Visual studio 2012 and GCC 4.8 are
// defined here
////////////////////////////////////////////////////////////////////////////


namespace std
{
#if (defined(_MSC_VER) && _MSC_VER <= 1700) || (defined(__GNUC__) && !__GNUC_PREREQ(4,9))
template<
    typename T>
    std::unique_ptr<T> make_unique()
{
    return std::unique_ptr<T>( new T );
}

template<
    typename T,
    typename Arg1>
    std::unique_ptr<T> make_unique( Arg1&& arg1 )
{
    return std::unique_ptr<T>( new T( std::forward<Arg1>( arg1 ) ) );
}

template<
    typename T,
    typename Arg1,
    typename Arg2>
    std::unique_ptr<T> make_unique( Arg1&& arg1, Arg2&& arg2 )
{
    return std::unique_ptr<T>( new T(
        std::forward<Arg1>( arg1 ),
        std::forward<Arg2>( arg2 ) ) );
}

template<
    typename T,
    typename Arg1,
    typename Arg2,
    typename Arg3>
    std::unique_ptr<T> make_unique( Arg1&& arg1, Arg2&& arg2, Arg3&& arg3 )
{
    return std::unique_ptr<T>( new T(
        std::forward<Arg1>( arg1 ),
        std::forward<Arg2>( arg2 ),
        std::forward<Arg3>( arg3 ) ) );
}

template<
    typename T,
    typename Arg1,
    typename Arg2,
    typename Arg3,
    typename Arg4>
    std::unique_ptr<T> make_unique( Arg1&& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4 )
{
    return std::unique_ptr<T>( new T(
        std::forward<Arg1>( arg1 ),
        std::forward<Arg2>( arg2 ),
        std::forward<Arg3>( arg3 ),
        std::forward<Arg4>( arg4 ) ) );
}
#endif
}   //std

namespace nx {

//TODO #ak move this out of here! std version will look like atomic<unique_ptr>
template<typename T>
class atomic_unique_ptr
{
    typedef void (atomic_unique_ptr<T>::*bool_type)() const;
    void this_type_does_not_support_comparisons() const {}

public:
    atomic_unique_ptr(T* ptr = nullptr)
    :
        m_ptr(ptr)
    {
    }
    atomic_unique_ptr(atomic_unique_ptr&& rhs)
    {
        auto ptr = rhs.m_ptr.exchange(nullptr);
        m_ptr.store(ptr);
    }
    ~atomic_unique_ptr()
    {
        auto ptr = m_ptr.exchange(nullptr);
        delete ptr;
    }

    void reset(T* ptr = nullptr)
    {
        auto oldPtr = m_ptr.exchange(ptr);
        delete oldPtr;
    }
    T* release()
    {
        return m_ptr.exchange(nullptr);
    }
    T* get()
    {
        return m_ptr.load();
    }
    const T* get() const
    {
        return m_ptr.load();
    }

    template<typename D>
    atomic_unique_ptr& operator=(atomic_unique_ptr<D>&& rhs)
    {
        if (this == rhs)
            return *this;

        reset(rhs.release());
        return *this;
    }
    template<typename D>
    atomic_unique_ptr& operator=(std::unique_ptr<D>&& rhs)
    {
        reset(rhs.release());
        return *this;
    }
    T* operator->()
    {
        return get();
    }
    const T* operator->() const
    {
        return get();
    }

    operator bool_type() const
    {
        return m_ptr.load()
            ? &atomic_unique_ptr<T>::this_type_does_not_support_comparisons
            : nullptr;
    }

private:
    std::atomic<T*> m_ptr;
};

}   //nx

#endif  //libcommon_cpp14_h
