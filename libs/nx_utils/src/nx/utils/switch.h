#pragma once

#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

struct Default {} default_;

template<typename Value, typename Action>
auto switch_(const Value&, const Default&, const Action& defaultAction)
    -> decltype(defaultAction())
{
    return defaultAction();
}

/**
 * The switch statement for any types and return values.
 *
 * Usage:
 * ```
 *  const std::string line = nx::utils::switch_(
 *      count,
 *      0, []{ return "none"; },
 *      1, []{ return "single"; },
 *      mux::default_, []{ return "many"; }
 *  );
 * ```
 *
 * Equivalent of:
 * ```
 * std::string line;
 * switch (count)
 * {
 *     case 0: line = "none";
 *     case 1: line = "single";
 *     default: line = "many";
 * }
 *
 * But keeps `line` const and supports `count` of any type.
 * ```
 */
template<typename Value, typename Match, typename Action, typename... Tail>
auto switch_(const Value& value, const Match& match, const Action& action,
    Tail... tail) -> decltype(action())
{
    if (value == match)
        return action();

    if constexpr (sizeof...(tail) > 0)
        return switch_(value, std::forward<Tail>(tail)...);

    NX_ASSERT(false, "Unmatched switch value");
    return decltype(action())();
}

/**
 * Enum safe switch, asserts if non of the cases is selected.
 *
 * Usage:
 * ```
 * // Asserts if `value == Enum::value3` or contains some other garbage.
 * const std::string& string = NX_ENUM_SWITCH(value,
 * {
 *     case Enum::value1: return "value1";
 *     case Enum::value2: return "value2";
 * });
 * ```
 */
#define NX_ENUM_SWITCH(VALUE, BODY) ( \
    [&] { \
        const auto mux_enum_switch_value_ = (VALUE); \
        switch(mux_enum_switch_value_) \
        BODY \
        NX_CRITICAL(false, lm("Unmatched switch value: %1").arg( \
            static_cast<int>(mux_enum_switch_value_))); \
        throw std::exception(); /*< Insted of required return. */ \
    }() \
)

} // namespace utils
} // namespace nx
