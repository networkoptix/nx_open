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
    RawDataParserFactory();
    AbstractRawDataParserPtr create();
    void setFactoryFunction(RawDataParserFactoryFunction function);

private:
    RawDataParserFactoryFunction m_defaultFactoryFunction;
    RawDataParserFactoryFunction m_factoryFunction = nullptr;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
