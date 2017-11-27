#include <gtest/gtest.h>
#include <nx/update/info/detail/data_parser/raw_data_parser_factory.h>
#include <nx/update/info/detail/data_parser/impl/json_data_parser.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
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
        ASSERT_NE(nullptr, dynamic_cast<impl::JsonDataParser*>(m_parser.get()));
    }

    void whenTestMetaDataParsed()
    {
        ASSERT_EQ(ResultCode::ok, m_parser->parseMetaData(metaDataJson, &m_updatesMetaData));
    }

    void thenUpdatesMetaDataShouldBeCorrect()
    {
        thenAlternativesServersShouldBeCorrect();
        thenCustomizationDataShouldBeCorrect();
    }


private:
    AbstractRawDataParserPtr m_parser;
    RawDataParserFactory m_parserFactory;
    UpdatesMetaData m_updatesMetaData;

    void thenAlternativesServersShouldBeCorrect()
    {
        ASSERT_EQ(1, m_updatesMetaData.alternativeServersDataList.size());
        AlternativeServerData alternativeServerData = m_updatesMetaData.alternativeServersDataList[0];
        ASSERT_EQ("mono-us", alternativeServerData.name);
        ASSERT_EQ("http://beta.networkoptix.com/beta-builds/daily/updates.json", alternativeServerData.url);
    }

    void thenCustomizationDataShouldBeCorrect()
    {
        ASSERT_EQ(29, m_updatesMetaData.customizationDataList.size());
    }
};

TEST_F(JsonDataParser, MetaDataParsedCorrectly)
{
    whenTestMetaDataParsed();
    thenUpdatesMetaDataShouldBeCorrect();
}

} // namespace test
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx



