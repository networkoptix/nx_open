#pragma once

/**
 *  Some c++14 features missing in MS Visual studio 2012 and GCC 4.8 are
 *  defined here
 */
#include <atomic>
#include <memory>

#ifndef __clang__

#ifdef __GNUC__
    #include <features.h>
#endif

namespace std {

#if defined(_MSC_VER)
    #if _MSC_VER <= 1700
        #define USE_OWN_MAKE_UNIQUE
    #endif
#elif defined(__GNUC_PREREQ)
    #if !__GNUC_PREREQ(4,9)
        #define USE_OWN_MAKE_UNIQUE
    #endif
#elif defined(__ANDROID__)
    #if !__GNUC_PREREQ__(4,9)
        #define USE_OWN_MAKE_UNIQUE
    #endif
#endif

#ifdef USE_OWN_MAKE_UNIQUE
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
#endif

#if defined(__GNUC__) && ATOMIC_INT_LOCK_FREE < 2 \
    && !__GNUC_PREREQ(7,0) //< see bits/exception_ptr.h
    typedef std::shared_ptr<exception> exception_ptr;

    template <typename E>
    exception_ptr make_exception_ptr(E e) { return std::make_shared<E>(std::move(e)); }

    inline
    exception_ptr current_exception()
    {
        try { throw; }
        catch (std::exception e) { return make_exception_ptr(std::move(e)); }
        return exception_ptr();
    }

    inline
    void rethrow_exception(exception_ptr p) { throw *p; }
#endif

} // namespace std

#endif // __clang__
