#include <nx/update/info/impl/common_update_registry.h>
#include <gtest/gtest.h>


namespace nx {
namespace update {
namespace info {
namespace impl {
namespace test {

static const QString kBaseUrl = "some.url";
static const QString kManualFile1 = "manual.file.1";
static const QString kManualFile2 = "manual.file.2";

static const QnSoftwareVersion kNxVersion{"4.0.0.0"};
static const OsVersion kOsVersion = ubuntuX64();

class CommonUpdateRegistry: public ::testing::Test
{
protected:
    CommonUpdateRegistry():
        m_registry(kBaseUrl, createMetaData(), createUpdateData())
    {

    }

    void whenManualDataAdded(const QList<QnUuid>& additionalPeers)
    {
        auto newManualData = ManualFileData(kManualFile1, kOsVersion, kNxVersion, false);
        newManualData.peers = additionalPeers;
        m_registry.addFileData(newManualData);
    }

    void thenAllPeersShouldBePresent(const QList<QnUuid>& additionalPeers)
    {
        UpdateFileRequestData request;

        request.isClient = false;
        request.currentNxVersion = QnSoftwareVersion("3.1.0");
        request.osVersion = kOsVersion;

        ASSERT_EQ(ResultCode::ok, m_registry.latestUpdate(request, nullptr));

        FileData fileData;
        ASSERT_EQ(ResultCode::ok, m_registry.findUpdateFile(request, &fileData));
        ASSERT_EQ(additionalPeers, m_registry.additionalPeers(fileData.file));
    }

private:
    impl::CommonUpdateRegistry m_registry;

    static detail::data_parser::UpdatesMetaData createMetaData()
    {
        return detail::data_parser::UpdatesMetaData();
    }

    static detail::CustomizationVersionToUpdate createUpdateData()
    {
        return detail::CustomizationVersionToUpdate();
    }
};

TEST_F(CommonUpdateRegistry, manualData)
{
    QList<QnUuid> ids = QList<QnUuid>() << QnUuid::createUuid() << QnUuid::createUuid();
    whenManualDataAdded(ids);
    whenManualDataAdded(ids << QnUuid::createUuid());
    thenAllPeersShouldBePresent(ids);
}

} // namespace test
} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
