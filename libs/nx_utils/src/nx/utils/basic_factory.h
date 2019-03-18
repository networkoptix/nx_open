#pragma once

#include <memory>
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

    /**
     * Set type to produce.
     * Product constructor must follow factory's signature.
     */
    template<typename Product>
    auto setCustomProduct()
    {
        return setCustomFunc(
            [](auto&&... args) { return std::make_unique<Product>(std::move(args)...); });
    }

private:
    Function m_func;
};

//-------------------------------------------------------------------------------------------------

template<
    typename AbstractType,
    typename DefaultInstanceType,
    typename... Args
>
// requires std::is_base_of<AbstractType, DefaultInstanceType>::value
class BasicAbstractObjectFactory:
    public nx::utils::BasicFactory<std::unique_ptr<AbstractType>(Args...)>
{
    using base_type = nx::utils::BasicFactory<std::unique_ptr<AbstractType>(Args...)>;

public:
    BasicAbstractObjectFactory():
        base_type(
            [this](auto&&... args)
            {
                return defaultFactory(std::forward<decltype(args)>(args)...);
            })
    {
    }

private:
    template<typename... LocalArgs>
    std::unique_ptr<AbstractType> defaultFactory(LocalArgs&&... args)
    {
        return std::make_unique<DefaultInstanceType>(
            std::forward<decltype(args)>(args)...);
    }
};

} // namespace utils
} // namespace nx
