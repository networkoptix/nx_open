#ifndef OBJECT_COUNTER
#define OBJECT_COUNTER

#if 1
#  include <time.h>
#  define debug_printf(...) fprintf(stdout, "<%.1f> ", clock()/100000.0), fprintf(stdout, __VA_ARGS__)
#else
#  define debug_printf(...)
//inline void debug_printf(const char * , ...) {}
#endif

#ifdef COUNT_OBJECTS

#include <atomic>

#define INIT_OBJECT_COUNTER(T) \
    template <> std::atomic_uint ObjectCounter<T>::ctor_(0); \
    template <> std::atomic_uint ObjectCounter<T>::dtor_(0); \
    template <> const char * ObjectCounter<T>::name_ = #T;

namespace ite
{
    template <typename T>
    class ObjectCounter
    {
    public:
        ObjectCounter<T>()
        {
            ++ctor_;
        }

        ~ObjectCounter<T>()
        {
            ++dtor_;
        }

        static const char * name() { return name_; }
        static unsigned ctorCount() { return ctor_; }
        static unsigned dtorCount() { return dtor_; }
        static unsigned diffCount() { return ctor_ - dtor_; }

    private:
        static const char * name_;
        static std::atomic_uint ctor_;
        static std::atomic_uint dtor_;
    };
}

#else

namespace ite
{
    template<typename T>
    class ObjectCounter
    {};
}

#endif

#endif
