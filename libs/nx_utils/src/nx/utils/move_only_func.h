// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <type_traits>

#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

/**
 * Move-only analogue of std::function.
 * Can be used to store lambda which has std::unique_ptr as its capture.
 */
template<class F>
class MoveOnlyFunc:
    private std::function<F>
{
    using base_type = std::function<F>;

    template<class Func>
    class MoveOnlyFuncWrapper
    {
    private:
        Func m_func;

    public:
        MoveOnlyFuncWrapper(Func p):
            m_func(std::move(p))
        {
        }

        MoveOnlyFuncWrapper(const MoveOnlyFuncWrapper& rhs):
            m_func(std::move(const_cast<MoveOnlyFuncWrapper&>(rhs).m_func))
        {
            NX_ASSERT(false);
        }

        MoveOnlyFuncWrapper& operator=(const MoveOnlyFuncWrapper&)
        {
            NX_ASSERT(false);
            return *this;
        }

        MoveOnlyFuncWrapper(MoveOnlyFuncWrapper&& rhs) noexcept(
            std::is_nothrow_move_constructible_v<Func>)
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
    using result_type = typename std::function<F>::result_type;

    MoveOnlyFunc() = default;

    MoveOnlyFunc(std::function<F> func):
        std::function<F>(std::move(func))
    {
    }

    MoveOnlyFunc(std::nullptr_t val):
        std::function<F>(val)
    {
    }

    MoveOnlyFunc(MoveOnlyFunc&&) = default;
    MoveOnlyFunc& operator=(MoveOnlyFunc&&) = default;
    MoveOnlyFunc(const MoveOnlyFunc&) = delete;
    MoveOnlyFunc& operator=(const MoveOnlyFunc&) = delete;

    template<class _Func>
    MoveOnlyFunc(_Func func):
        std::function<F>(MoveOnlyFuncWrapper<_Func>(std::move(func)))
    {
    }

    MoveOnlyFunc& operator=(std::nullptr_t val)
    {
        std::function<F>::operator=(val);
        return *this;
    }

    using base_type::operator();

    bool operator==(std::nullptr_t) const
    {
        return static_cast<const base_type&>(*this) == nullptr;
    }

    bool operator!=(std::nullptr_t) const
    {
        return !(*this == nullptr);
    }

    void swap(MoveOnlyFunc& other)
    {
        base_type::swap(static_cast<base_type&>(other));
    }

    using std::function<F>::operator bool;
};

template<typename Function, typename ... Args>
void moveAndCall(Function& function, Args&& ... args)
{
    const auto handler = std::move(function);
    function = nullptr;
    handler(std::forward<Args>(args) ...);
}

template<typename Function, typename ... Args>
void moveAndCallOptional(Function& function, Args&& ... args)
{
    if (!function)
        return;

    moveAndCall(function, std::forward<Args>(args) ...);
}

template<typename Function, typename ... Args>
void swapAndCall(Function& function, Args&& ... args)
{
    Function functionLocal;
    function.swap(functionLocal);
    functionLocal(std::forward<Args>(args) ...);
}

} // namespace utils
} // namespace nx
