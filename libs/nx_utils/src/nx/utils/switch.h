#pragma once

#include <nx/utils/log/assert.h>

namespace nx::utils {

static constexpr struct Default {} default_;

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
 *      0, []{ return "none"; },
 *      1, []{ return "single"; },
 *      nx::utils::default_, []{ return "many"; }
 *  );
 * ```
 * Equivalent of:
 * ```
 *  std::string line;
 *  switch (count)
 *  {
 *      case 0:
 *          line = "none";
 *          break;
 *      case 1:
 *          line = "single";
 *          break;
 *      default:
 *          line = "many";
 *          break;
 *  }
 * ```
 * Advantage here: keeps `line` const.
 *
 * # Usage example 2:
 * ```
 *  nx::utils::switch_(data.command(),
 *      "help", []{ printHelp(); },
 *      "save", [&]{ save(data.argument()); },
 *  );
 * ```
 * Equivalent of:
 * ```
 *  const auto inputValue = data.command();
 *  if (inputValue == "help")
 *      printHelp();
 *  else if (inputValue == "save")
 *      save(data.argument());
 *  else
 *      NX_ASSERT(false, "Unmatched switch value");
 * ```
 * Advantages here: supports switching by value of any type; run-time check for unmatched values.
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

} // namespace nx::utils
