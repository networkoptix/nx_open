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
static const QString kManualFile3 = "manual.file.3";

static const QnSoftwareVersion kNxVersion{"4.0.0.0"};
static const OsVersion kOsVersion1 = ubuntuX64();
static const OsVersion kOsVersion2 = ubuntuX86();

class CommonUpdateRegistry: public ::testing::Test
{
protected:
    CommonUpdateRegistry():
        m_registry(kBaseUrl, createMetaData(), createUpdateData()),
        m_registry2(kBaseUrl, createMetaData(), createUpdateData())
    {
    }

    enum RegistryNum
    {
        first,
        second
    };

    void whenManualDataAdded(
        const QString& fileName,
        const OsVersion& osVersion,
        const QList<QnUuid>& additionalPeers,
        RegistryNum registryNum = RegistryNum::first)
    {
        auto newManualData = ManualFileData(fileName, osVersion, kNxVersion, false);
        newManualData.peers = additionalPeers;

        switch (registryNum)
        {
        case RegistryNum::first:
            m_registry.addFileData(newManualData);
            break;
        case RegistryNum::second:
            m_registry2.addFileData(newManualData);
        }
    }

    void thenAllPeersShouldBePresent(
        const OsVersion& osVersion,
        const QList<QnUuid>& additionalPeers)
    {
        UpdateFileRequestData request;

        request.isClient = false;
        request.currentNxVersion = QnSoftwareVersion("3.1.0");
        request.osVersion = osVersion;

        ASSERT_EQ(ResultCode::ok, m_registry.latestUpdate(request, nullptr));

        auto referencePeers = additionalPeers;
        std::sort(
            referencePeers.begin(),
            referencePeers.end(),
            [](const QnUuid& l, const QnUuid& r) { return l < r; });

        FileData fileData;
        ASSERT_EQ(ResultCode::ok, m_registry.findUpdateFile(request, &fileData));
        ASSERT_EQ(referencePeers, m_registry.additionalPeers(fileData.file));
    }

    void whenManualDataRemoved(const QString& fileName)
    {
        m_registry.removeFileData(fileName);
    }

    void thenManualDataShouldNotBeFound(const OsVersion& version1)
    {
        UpdateFileRequestData request;

        request.isClient = false;
        request.currentNxVersion = QnSoftwareVersion("3.1.0");
        request.osVersion = version1;

        FileData fileData;
        ASSERT_EQ(ResultCode::ok, m_registry.latestUpdate(request, nullptr));
        ASSERT_EQ(ResultCode::noData, m_registry.findUpdateFile(request, &fileData));
    }

    void whenRegistriesAreMerged()
    {
        m_mergedRegistry1to2 = doMerge(m_registry, m_registry2);
        m_mergedRegistry2to1 = doMerge(m_registry2, m_registry);
    }

    void thenMergedRegistriesShouldBeEqual()
    {
        ASSERT_TRUE(m_mergedRegistry1to2.equals(&m_mergedRegistry2to1));
        ASSERT_TRUE(m_mergedRegistry2to1.equals(&m_mergedRegistry1to2));
    }

    void andWhenManualDataRemovedFromMergedRegistry(RegistryNum removeFrom, const QString& fileName)
    {
        impl::CommonUpdateRegistry* to, *from;
        switch (removeFrom)
        {
        case RegistryNum::first:
            from = &m_mergedRegistry1to2;
            to = &m_mergedRegistry2to1;
            break;
        case RegistryNum::second:
            from = &m_mergedRegistry2to1;
            to = &m_mergedRegistry1to2;
            break;
        }

        from->removeFileData(fileName);
        ASSERT_EQ(from->additionalPeers(fileName), QList<QnUuid>());

        to->merge(from);
    }

private:
    impl::CommonUpdateRegistry m_registry;
    impl::CommonUpdateRegistry m_registry2;
    impl::CommonUpdateRegistry m_mergedRegistry1to2;
    impl::CommonUpdateRegistry m_mergedRegistry2to1;

    static detail::data_parser::UpdatesMetaData createMetaData()
    {
        return detail::data_parser::UpdatesMetaData();
    }

    static detail::CustomizationVersionToUpdate createUpdateData()
    {
        return detail::CustomizationVersionToUpdate();
    }

    static impl::CommonUpdateRegistry doMerge(
        const impl::CommonUpdateRegistry& to,
        const impl::CommonUpdateRegistry& from)
    {
        impl::CommonUpdateRegistry _to = to;
        impl::CommonUpdateRegistry _from = from;
        _to.merge(&_from);

        return _to;
    }
};

TEST_F(CommonUpdateRegistry, manualData)
{
    QList<QnUuid> ids1 = QList<QnUuid>() << QnUuid::createUuid() << QnUuid::createUuid();
    whenManualDataAdded(kManualFile1, kOsVersion1, ids1);

    QList<QnUuid> ids2 = QList<QnUuid>() << QnUuid::createUuid();

    whenManualDataAdded(kManualFile2, kOsVersion2, ids2);

    thenAllPeersShouldBePresent(kOsVersion1, ids1);
    thenAllPeersShouldBePresent(kOsVersion2, ids2);

    whenManualDataRemoved(kManualFile1);
    thenManualDataShouldNotBeFound(kOsVersion1);
    thenAllPeersShouldBePresent(kOsVersion2, ids2);
}

TEST_F(CommonUpdateRegistry, manualData_merge)
{
    whenManualDataAdded(
        kManualFile1,
        kOsVersion1,
        QList<QnUuid>() << QnUuid::createUuid() << QnUuid::createUuid());

    whenManualDataAdded(
        kManualFile2,
        kOsVersion2,
        QList<QnUuid>() << QnUuid::createUuid());

    whenManualDataAdded(
        kManualFile3,
        kOsVersion2,
        QList<QnUuid>() << QnUuid::createUuid(),
        RegistryNum::second);

    whenRegistriesAreMerged();
    thenMergedRegistriesShouldBeEqual();

    andWhenManualDataRemovedFromMergedRegistry(RegistryNum::first, kManualFile3);
    thenMergedRegistriesShouldBeEqual();

    andWhenManualDataRemovedFromMergedRegistry(RegistryNum::second, kManualFile1);
    thenMergedRegistriesShouldBeEqual();
}

} // namespace test
} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
