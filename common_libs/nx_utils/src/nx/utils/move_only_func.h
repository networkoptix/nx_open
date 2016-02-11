/**********************************************************
* Feb 11, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>


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
            assert(false);
            throw -1;
        }
        MoveOnlyFuncWrapper& operator=(const MoveOnlyFuncWrapper&)
        {
            assert(false);
            throw -1;
        }
        MoveOnlyFuncWrapper(MoveOnlyFuncWrapper&&) = default;
        MoveOnlyFuncWrapper& operator=(MoveOnlyFuncWrapper&&) = default;

        template<class... Args>
        auto operator() (Args&&... args)
            -> decltype(m_func(std::forward<Args>(args)...))
        {
            return m_func(std::forward<Args>(args)...);
        }
    };

public:
    MoveOnlyFunc() = default;
    
    template<class _Func>
    MoveOnlyFunc(_Func&& func)
    :
        std::function<F>(
            MoveOnlyFuncWrapper<typename std::remove_reference<_Func>::type>(
                std::forward<_Func>(func)))
    {
    }

    MoveOnlyFunc(MoveOnlyFunc&&) = default;
    MoveOnlyFunc& operator=(MoveOnlyFunc&&) = default;
    MoveOnlyFunc(const MoveOnlyFunc&) = delete;
    MoveOnlyFunc& operator=(const MoveOnlyFunc&) = delete;

    using std::function<F>::operator();
    using std::function<F>::operator bool;
};

}   //utils
}   //nx
