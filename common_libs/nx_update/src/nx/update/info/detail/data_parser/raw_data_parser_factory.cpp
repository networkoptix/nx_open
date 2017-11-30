#include "raw_data_parser_factory.h"
#include "abstract_raw_data_parser.h"
#include "impl/json_data_parser.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

RawDataParserFactoryFunction RawDataParserFactory::m_factoryFunction = nullptr;

namespace {

static AbstractRawDataParserPtr createJsonParser()
{
    return AbstractRawDataParserPtr(new impl::JsonDataParser());
}

} // namespace


AbstractRawDataParserPtr RawDataParserFactory::create()
{
    if (m_factoryFunction != nullptr)
        return m_factoryFunction();

    return createJsonParser();
}

void RawDataParserFactory::setFactoryFunction(RawDataParserFactoryFunction function)
{
    m_factoryFunction = std::move(function);
}

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
