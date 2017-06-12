#pragma once

#include <type_traits>

#include <nx/utils/move_only_func.h>

namespace nx {
namespace utils {

template<typename FactoryFunc>
class BasicFactory
{
public:
    using Function = nx::utils::MoveOnlyFunc<FactoryFunc>;

    BasicFactory(Function defaultFunc):
        m_func(std::move(defaultFunc))
    {
    }

    template<typename ... Args>
    typename std::result_of<Function(Args...)>::type create(Args&& ... args)
    {
        return m_func(std::forward<Args>(args)...);
    }

    Function setCustomFunc(Function func)
    {
        m_func.swap(func);
        return func;
    }

private:
    Function m_func;
};

} // namespace utils
} // namespace nx
