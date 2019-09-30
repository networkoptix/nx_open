#include "types.h"

#include <nx/utils/string.h>

namespace nx::cloud::storage::client {

std::string toString(const DeviceDescription& value)
{
    return nx::utils::join(
        value.begin(), value.end(),
        "\r\n",
        std::mem_fn(&Parameter::name), '=', std::mem_fn(&Parameter::value));
}

void fromString(
    const std::string_view& str,
    DeviceDescription* value)
{
    nx::utils::split(
        str, nx::utils::separator::isAnyOf("\r\n"),
        [&value](const std::string_view& str)
        {
            const auto [tokens, tokenCount] = nx::utils::split_n<2>(str, '=');
            value->push_back(Parameter{std::string(tokens[0]), std::string(tokens[1])});
        });
}

} // namespace nx::cloud::storage::client
