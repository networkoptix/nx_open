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

static const QnUuid kPeer1Id = QnUuid::createUuid();
static const QnUuid kPeer2Id = QnUuid::createUuid();

class CommonUpdateRegistry: public ::testing::Test
{
protected:
    CommonUpdateRegistry():
        m_registry(kPeer1Id, kBaseUrl, createMetaData(), createUpdateData()),
        m_registry2(kPeer2Id, kBaseUrl, createMetaData(), createUpdateData())
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
        RegistryNum registryNum = RegistryNum::first)
    {
        auto newManualData = ManualFileData(fileName, osVersion, kNxVersion, false);

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

    void thenManualDataShouldNotBeFound(const OsVersion& version)
    {
        assertDataPresence(version, /* shouldBePresent*/ false);
    }

    void thenManualDataShouldBeFound(const OsVersion& version)
    {
        assertDataPresence(version, /* shouldBePresent*/ true);
    }

    void whenRegistriesAreMerged()
    {
        m_mergedRegistry1to2 = doMerge(m_registry, m_registry2);
        m_mergedRegistry2to1 = doMerge(m_registry2, m_registry);
    }

    void thenMergedRegistriesShouldBeEqual()
    {
        ASSERT_TRUE(m_mergedRegistry1to2->equals(m_mergedRegistry2to1.get()));
        ASSERT_TRUE(m_mergedRegistry2to1->equals(m_mergedRegistry1to2.get()));
    }

    void andWhenManualDataRemovedFromMergedRegistry(
        RegistryNum removeFrom,
        const QString& fileName,
        const QList<QnUuid>& expectedRestPeers)
    {
        impl::CommonUpdateRegistry* to, *from;
        switch (removeFrom)
        {
        case RegistryNum::first:
            from = m_mergedRegistry1to2.get();
            to = m_mergedRegistry2to1.get();
            break;
        case RegistryNum::second:
            from = m_mergedRegistry2to1.get();
            to = m_mergedRegistry1to2.get();
            break;
        }

        from->removeFileData(fileName);
        ASSERT_EQ(from->additionalPeers(fileName), expectedRestPeers);

        to->merge(from);
    }

    void thenMergedRegistriesShouldRemainEqualAfterSerialization()
    {
        assertEqualityAfterSerialization(m_mergedRegistry1to2.get());
        assertEqualityAfterSerialization(m_mergedRegistry2to1.get());
    }

private:
    impl::CommonUpdateRegistry m_registry;
    impl::CommonUpdateRegistry m_registry2;
    std::unique_ptr<impl::CommonUpdateRegistry> m_mergedRegistry1to2;
    std::unique_ptr<impl::CommonUpdateRegistry> m_mergedRegistry2to1;

    static detail::data_parser::UpdatesMetaData createMetaData()
    {
        return detail::data_parser::UpdatesMetaData();
    }

    static detail::CustomizationVersionToUpdate createUpdateData()
    {
        return detail::CustomizationVersionToUpdate();
    }

    static std::unique_ptr<impl::CommonUpdateRegistry> doMerge(
        const impl::CommonUpdateRegistry& to,
        const impl::CommonUpdateRegistry& from)
    {
        std::unique_ptr<impl::CommonUpdateRegistry> _to =
            std::unique_ptr<impl::CommonUpdateRegistry>(new impl::CommonUpdateRegistry(to));

        impl::CommonUpdateRegistry _from = from;
        _to->merge(&_from);

        return _to;
    }

    void assertDataPresence(const OsVersion& version, bool shouldBePresent)
    {
        UpdateFileRequestData request;

        request.isClient = false;
        request.currentNxVersion = QnSoftwareVersion("3.1.0");
        request.osVersion = version;

        FileData fileData;
        if (shouldBePresent)
            ASSERT_EQ(ResultCode::ok, m_registry.findUpdateFile(request, &fileData));
        else
            ASSERT_EQ(ResultCode::noData, m_registry.findUpdateFile(request, &fileData));
    }

    void assertEqualityAfterSerialization(impl::CommonUpdateRegistry* registry)
    {
        auto buf = registry->toByteArray();
        impl::CommonUpdateRegistry restoredRegistry(kPeer1Id);
        restoredRegistry.fromByteArray(buf);

        ASSERT_TRUE(registry->equals(&restoredRegistry));
    }
};

TEST_F(CommonUpdateRegistry, manualData_remove)
{
    whenManualDataAdded(kManualFile1, kOsVersion1);
    whenManualDataAdded(kManualFile2, kOsVersion2);

    thenAllPeersShouldBePresent(kOsVersion1, QList<QnUuid>() << kPeer1Id);
    thenAllPeersShouldBePresent(kOsVersion2, QList<QnUuid>() << kPeer1Id);

    whenManualDataRemoved(kManualFile2);
    thenManualDataShouldNotBeFound(kOsVersion2);
    thenAllPeersShouldBePresent(kOsVersion1, QList<QnUuid>() << kPeer1Id);

    whenManualDataRemoved(kManualFile1);
    thenManualDataShouldNotBeFound(kOsVersion1);
}

TEST_F(CommonUpdateRegistry, manualData_merge)
{
    whenManualDataAdded(
        kManualFile1,
        kOsVersion1);

    whenManualDataAdded(
        kManualFile2,
        kOsVersion2);

    whenManualDataAdded(
        kManualFile3,
        kOsVersion2,
        RegistryNum::second);

    whenRegistriesAreMerged();
    thenMergedRegistriesShouldBeEqual();

    andWhenManualDataRemovedFromMergedRegistry(
        RegistryNum::first,
        kManualFile3,
        QList<QnUuid>() << kPeer2Id);
    thenMergedRegistriesShouldBeEqual();

    andWhenManualDataRemovedFromMergedRegistry(
        RegistryNum::second,
        kManualFile1,
        QList<QnUuid>() << kPeer1Id);
    thenMergedRegistriesShouldBeEqual();

    andWhenManualDataRemovedFromMergedRegistry(
        RegistryNum::first,
        kManualFile1,
        QList<QnUuid>());
    thenMergedRegistriesShouldBeEqual();

    thenMergedRegistriesShouldRemainEqualAfterSerialization();
}

} // namespace test
} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
