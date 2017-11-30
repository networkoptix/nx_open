#pragma once

#include <nx/update/info/detail/fwd.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

class RawDataParserFactory
{
public:
    static AbstractRawDataParserPtr create();
    static void setFactoryFunction(RawDataParserFactoryFunction function);

private:
    static RawDataParserFactoryFunction m_factoryFunction;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
