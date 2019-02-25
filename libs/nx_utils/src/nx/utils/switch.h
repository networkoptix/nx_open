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
 * # Usage example 1:
 * ```
 *  const std::string line = nx::utils::switch_(count,
 *      0, [](){ return "none"; },
 *      1, [](){ return "single"; },
 *      nx::utils::default_, [](){ return "many"; }
 *  );
 * ```
 * Equivalent of:
 * ```
 *  std::string line;
 *  switch (count)
 *  {
 *      case 0: line = "none";
 *      case 1: line = "single";
 *      default: line = "many";
 *  }
 * ```
 * Advantage here: keeps `line` const.
 *
 * # Usage example 2:
 * ```
 *  nx::utils::switch_(input.read(),
 *      "help", [](){ printHelp(); },
 *      "save", [&](){ save(value.toInt()); },
 *  );
 * ```
 * Equivalent of:
 * ```
 *  const auto inputValue = input.read();
 *  if (inputValue == "help")
 *      printHelp();
 *  else if (inputValue == "save")
 *      save(value.toInt());
 *  else
 *      NX_ASSERT(false, "Unmatched switch value");
 * ```
 * Advantages here: supports switching by value of any type; run-time check for umnatched values.
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

} // namespace utils
} // namespace nx
