#include "squid_proxy_emulator.h"

#include <nx/utils/byte_stream/string_replacer.h>

namespace nx::network::test {

SquidProxyEmulator::SquidProxyEmulator()
{
    setDownStreamConverterFactory(
        []() { return std::make_unique<nx::utils::bstream::StringReplacer>(
            "Connection: Upgrade\r\n", ""); });
}

} // namespace nx::network::test
