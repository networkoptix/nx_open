#include <algorithm>
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

class MetaDataParser: public ::testing::Test
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
        checkCustomizationContent("digitalwatchdog", 8, "17.2.3.15624");
        checkCustomizationContent("win4net", 4, "2.5.0.11376");
        checkCustomizationContent("cox", 4, "17.2.3.15624");
    }

    void checkCustomizationContent(
        const QString& customizationName,
        int versionsCount,
        const QString& lastVersion)
    {
        auto customizationIt = std::find_if(
            m_updatesMetaData.customizationDataList.cbegin(),
            m_updatesMetaData.customizationDataList.cend(),
            [&customizationName](const CustomizationData& data)
            {
                return data.name == customizationName;
            });

        ASSERT_NE(m_updatesMetaData.customizationDataList.cend(), customizationIt);
        ASSERT_EQ(versionsCount, (int) customizationIt->versions.size());

        const auto& versions = customizationIt->versions;
        ASSERT_EQ(QnSoftwareVersion(lastVersion), versions[versions.size() - 1]);
    }
};

TEST_F(MetaDataParser, MetaDataParsedCorrectly)
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



