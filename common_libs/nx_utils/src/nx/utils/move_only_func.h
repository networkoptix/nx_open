/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <cassert>
#include <functional>
#include <nx/utils/log/assert.h>


namespace nx {
namespace utils {

/** Move-only analogue of \a std::function.
    Can be used to store lambda which has \a std::unique_ptr as its capture
*/
template<class F>
class MoveOnlyFunc
:
    private std::function<F>
{
    template<class Func>
    class MoveOnlyFuncWrapper
    {
    private:
        Func m_func;

    public:
        MoveOnlyFuncWrapper(Func p)
        :
            m_func(std::move(p))
        {
        }
        MoveOnlyFuncWrapper(const MoveOnlyFuncWrapper& rhs)
        :
            m_func(std::move(const_cast<MoveOnlyFuncWrapper&>(rhs).m_func))
        {
            NX_ASSERT(false);
        }
        MoveOnlyFuncWrapper& operator=(const MoveOnlyFuncWrapper&)
        {
            NX_ASSERT(false);
        }
        MoveOnlyFuncWrapper(MoveOnlyFuncWrapper&& rhs)
        :
            m_func(std::move(rhs.m_func))
        {
        }
        MoveOnlyFuncWrapper& operator=(MoveOnlyFuncWrapper&& rhs)
        {
            if (&rhs == this)
                return *this;
            m_func = std::move(rhs.m_func);
            return *this;
        }

        template<class... Args>
        auto operator() (Args&&... args)
            -> decltype(m_func(std::forward<Args>(args)...))
        {
            return m_func(std::forward<Args>(args)...);
        }
    };

public:
    MoveOnlyFunc() = default;
    
    MoveOnlyFunc(std::function<F> func)
    :
        std::function<F>(std::move(func))
    {
    }

    MoveOnlyFunc(std::nullptr_t val)
    :
        std::function<F>(val)
    {
    }

    MoveOnlyFunc(MoveOnlyFunc&&) = default;
    MoveOnlyFunc& operator=(MoveOnlyFunc&&) = default;
    MoveOnlyFunc(const MoveOnlyFunc&) = delete;
    MoveOnlyFunc& operator=(const MoveOnlyFunc&) = delete;

    template<class _Func>
    MoveOnlyFunc(_Func func)
    :
        std::function<F>(MoveOnlyFuncWrapper<_Func>(std::move(func)))
    {
    }

    template<class _Func>
    const MoveOnlyFunc& operator=(_Func func)
    {
        auto x = MoveOnlyFuncWrapper<_Func>(std::move(func));
        std::function<F>::operator=(std::move(x));
        return *this;
    }

    MoveOnlyFunc& operator=(std::nullptr_t val)
    {
        std::function<F>::operator=(val);
        return *this;
    }

    using std::function<F>::operator();
    using std::function<F>::operator bool;
};

}   //utils
}   //nx
