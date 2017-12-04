#include <algorithm>
#include <gtest/gtest.h>
#include <nx/update/info/detail/data_parser/raw_data_parser_factory.h>
#include <nx/update/info/detail/data_parser/impl/json_data_parser.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/fwd.h>

#include "../../../inl.h"

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
        m_parser = RawDataParserFactory::create();
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

class UpdateDataParser: public ::testing::Test
{
protected:
    UpdateDataParser(): m_parser(m_parserFactory.create()) {}

    void whenUpdateDataParsed()
    {
        ASSERT_EQ(ResultCode::ok, m_parser->parseUpdateData(updateJson, &m_updateData));
    }

    void thenUpdateDataShouldBeCorrect()
    {
        thenCloudHostShoudBeCorrect();
        thenServerPackagesShouldBeCorrect();
        thenClientPackagesShouldBeCorrect();
    }

private:
    RawDataParserFactory m_parserFactory;
    AbstractRawDataParserPtr m_parser;
    UpdateData m_updateData;

    void thenCloudHostShoudBeCorrect()
    {
        ASSERT_EQ("nxvms.com", m_updateData.cloudHost);
    }

    void thenServerPackagesShouldBeCorrect()
    {
        ASSERT_EQ(7, m_updateData.targetToPackage.size());
        checkFileData(m_updateData.targetToPackage, "linux.arm_rpi", 80605107);
        checkFileData(m_updateData.targetToPackage, "windows.x86_winxp", 65677361);
    }

    void checkFileData(const QHash<QString, FileData>& targetToFileData, const QString& target, qint64 size)
    {
        auto fileDataIt = targetToFileData.find(target);
        ASSERT_NE(targetToFileData.constEnd(), fileDataIt);
        ASSERT_EQ(size, fileDataIt->size);
        ASSERT_FALSE(fileDataIt->file.isEmpty());
        ASSERT_FALSE(fileDataIt->md5.isEmpty());
    }

    void thenClientPackagesShouldBeCorrect()
    {
        ASSERT_EQ(5, m_updateData.targetToClientPackage.size());
        checkFileData(m_updateData.targetToClientPackage, "linux.x64_ubuntu", 103335409);
        checkFileData(m_updateData.targetToClientPackage, "macosx.x64_macosx", 77903157);
        checkFileData(m_updateData.targetToClientPackage, "windows.x64_winxp", 86289814);
    }
};

TEST_F(MetaDataParser, MetaDataParsedCorrectly)
{
    whenTestMetaDataParsed();
    thenUpdatesMetaDataShouldBeCorrect();
}

TEST_F(UpdateDataParser, UpdateDataParsedCorrectly)
{
    whenUpdateDataParsed();
    thenUpdateDataShouldBeCorrect();
}

} // namespace test
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx



