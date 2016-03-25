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
#ifdef _MSC_VER
#   if _MSC_VER <= 1700
#       define USE_OWN_MAKE_UNIQUE
#   endif
#elif defined(__GNUC_PREREQ)
#   if !__GNUC_PREREQ(4,9)
#      define USE_OWN_MAKE_UNIQUE
#   endif
#endif
#ifdef USE_OWN_MAKE_UNIQUE
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
#endif  //USE_OWN_MAKE_UNIQUE
}   //std

#endif  //libcommon_cpp14_h
