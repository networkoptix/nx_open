#include <gtest/gtest.h>
#include <nx/update/info/detail/data_parser/raw_data_parser_factory.h>
#include <nx/update/info/detail/data_parser/json_data_parser.h>
#include <nx/update/info/detail/fwd.h>

extern const char* const updateJson;
extern const char* const metaDataJson;

#include "test_json_data.inl"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {
namespace test {

class JsonDataParser: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_parser = m_parserFactory.create();
        ASSERT_TRUE((bool) m_parser.get());
        ASSERT_NE(nullptr, dynamic_cast<JsonDataParser*>(m_parser.get()));
    }

    void whenTestMetaDataParsed()
    {
        customizationInfoList = m_parser->parseMetaData(metaDataJson);
    }

    void thenCustomizationInfoListShouldBeCorrect()
    {
        ASSERT_NE(nullptr, customizationInfoList);
    }

private:
    AbstractRawDataParserPtr m_parser;
    RawDataParserFactory m_parserFactory;
    AbstractCustomizationInfoList* customizationInfoList = nullptr;
};

TEST_F(JsonDataParser, MetaDataParsedCorrectly)
{
    whenTestMetaDataParsed();
    thenCustomizationInfoListShouldBeCorrect();
}

} // namespace test
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx



