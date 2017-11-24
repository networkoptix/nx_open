#include "raw_data_parser_factory.h"
#include "abstract_raw_data_parser.h"
#include "json_data_parser.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

namespace {

static AbstractRawDataParserPtr createJsonParser()
{
    return AbstractRawDataParserPtr(new JsonDataParser());
}

} // namespace

RawDataParserFactory::RawDataParserFactory():
    m_defaultFactoryFunction(&createJsonParser)
{}

AbstractRawDataParserPtr RawDataParserFactory::create()
{
    if (m_factoryFunction)
        return m_factoryFunction();

    return m_defaultFactoryFunction();
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
