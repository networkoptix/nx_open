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
    template<class Fun>
    class MoveOnlyFuncWrapper
    {
    public:
        Fun f;

        MoveOnlyFuncWrapper(Fun p)
        :
            f(std::move(p))
        {
        }
        MoveOnlyFuncWrapper(const MoveOnlyFuncWrapper& rhs)
        :
            f(std::move(const_cast<MoveOnlyFuncWrapper&>(rhs).f))
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
            -> decltype(f(std::forward<Args>(args)...))
        {
            return f(std::forward<Args>(args)...);
        }
    };

public:
    MoveOnlyFunc() = default;
    
    template<class Fun>
    MoveOnlyFunc(Fun fun)
    :
        std::function<F>(MoveOnlyFuncWrapper<Fun>(std::move(fun)))
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
