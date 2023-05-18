// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <type_traits>

#include "accessor_traits.h"

namespace nx::utils::algorithm::detail {

/*
 * Makes correct call of the given accessor depending on the accessor type - callable functor
 * or pointer to the member field.
 */
template<typename A>
class Accessor
{
public:
    using Type = typename accessor_traits<A>::arg1::type;
    using ValueType = typename accessor_traits<A>::result;

    Accessor(const A& accessor):
        m_accessor(accessor)
    {
    }

    ValueType operator()(const Type& arg) const
    {
        if constexpr (std::is_member_object_pointer<A>::value)
            return arg.*m_accessor;
        else
            return m_accessor(arg);
    }

private:
    A m_accessor;
};

} // namespace nx::utils::algorithm::detail
