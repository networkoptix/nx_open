/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#ifndef libcommon_cpp14_h
#define libcommon_cpp14_h

#include <atomic>
#include <memory>

#ifndef __clang__

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
    typename T,
    typename... Args
> std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif  //USE_OWN_MAKE_UNIQUE
}   //std

#endif // __clang__

#endif  //libcommon_cpp14_h
