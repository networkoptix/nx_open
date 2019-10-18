#include "test_api_requests.h"

#include <nx/utils/log/log.h>
#include <common/common_module.h>
#include <nx/utils/random.h>
#include <utils/crypt/symmetrical.h>
#include <nx/utils/url.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_resource.h>
#include <transaction/amend_transaction_data.h>

#include <gtest/gtest.h>

namespace nx {
namespace test {

namespace {

/**
 * Assert that the list contains the storage with the specified name, retrieve this storage and
 * check its id and parentId if specified.
 */
#define NX_FIND_STORAGE_BY_NAME(...) ASSERT_NO_FATAL_FAILURE(findStorageByName(__VA_ARGS__))
static void findStorageByName(
    const vms::api::StorageDataList& storages,
    vms::api::StorageData* outStorage,
    const QString name,
    QnUuid parentId = QnUuid(),
    QnUuid id = QnUuid())
{
    ASSERT_TRUE(storages.size() > 0);

    const auto foundStorage = std::find_if(storages.cbegin(), storages.cend(),
        [name](const vms::api::StorageData& s) { return s.name == name; });

    ASSERT_TRUE(foundStorage != storages.cend());

    *outStorage = *foundStorage; //< Copy struct by value.

    if (!id.isNull())
    {
        ASSERT_EQ(id, outStorage->id);
    }

    if (!parentId.isNull())
    {
        ASSERT_EQ(parentId, outStorage->parentId);
    }
}

} // namespace

TEST(GetStorages, saveAndMerge)
{
    MediaServerLauncher launcher;
    launcher.addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    ASSERT_TRUE(launcher.start());

    vms::api::StorageDataList storages;
    vms::api::StorageData storage;

    NX_INFO(this, "Create a new storage with auto-generated id.");
    storage.name = "original name";
    storage.parentId = launcher.commonModule()->moduleGUID();
    storage.spaceLimit = 113326;
    storage.storageType = "local";
    NX_TEST_API_POST(&launcher,
        lit("/ec2/saveStorage"), storage, removeJsonFields({"id"}));

    NX_INFO(this, "Retrieve the created storage.");
    NX_TEST_API_GET(&launcher, lit("/ec2/getStorages"), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Rename the storage via Merge.");
    storage.name = "new name";
    NX_TEST_API_POST(&launcher,
        lit("/ec2/saveStorage"), storage, keepOnlyJsonFields({"id", "name"}));

    NX_INFO(this, "Check that the storage is renamed.");
    NX_TEST_API_GET(&launcher, lit("/ec2/getStorages"), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId, storage.id);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Check the storage can be found by its parent server id.");
    NX_TEST_API_GET(&launcher,
        lit("/ec2/getStorages?id=%1").arg(storage.parentId.toString()), &storages);
    NX_FIND_STORAGE_BY_NAME(storages, &storage, storage.name, storage.parentId, storage.id);
    ASSERT_EQ(1U, storages.size());

    NX_INFO(this, "Check that no storages are found by another (non-existing) parent server id.");
    NX_TEST_API_GET(&launcher,
        lit("/ec2/getStorages?id=%1").arg(QnUuid::createUuid().toString()), &storages);
    ASSERT_TRUE(storages.empty());
}

static nx::vms::api::ResourceParamDataList generateRandomParams(int count)
{
    using namespace nx::vms::api;
    using namespace nx::utils;
    ResourceParamDataList result;

    for (int i = 0; i < count; ++i)
        result.push_back(ResourceParamData{random::generateName(5), random::generateName(5)});

    return result;
}

struct Param
{
    QString url;

    Param(const QString& url): url(url) {}
};

class Storage: public ::testing::TestWithParam<Param>
{
protected:
    virtual void SetUp() override
    {
        whenServerStarted();
    }

    virtual void TearDown() override
    {
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(kDbConnectionName);
    }

    void whenStorageDataSaved(const nx::vms::api::StorageData& data)
    {
        NX_TEST_API_POST(&m_server, "/ec2/saveStorage", data, removeJsonFields({"id"}));
    }

    void thenItShouldBeRetrievableViaApiWithEncryptedCredentials(
        const nx::vms::api::StorageData& expected)
    {
        nx::vms::api::StorageDataList storages;
        NX_TEST_API_GET(&m_server, "/ec2/getStorages", &storages);
        const auto dataIt = std::find_if(
            storages.cbegin(), storages.cend(),
            [&expected](const auto& data) { return expected.name == data.name; });

        ASSERT_NE(storages.cend(), dataIt);
        ASSERT_EQ(expected.isBackup, dataIt->isBackup);

        #if 0 //< Enable when /ec2/saveStorage starts saving additional params.
            ASSERT_EQ(expected.addParams, dataIt->addParams);
        #endif

        ASSERT_EQ(expected.spaceLimit, dataIt->spaceLimit);
        ASSERT_EQ(expected.storageType, dataIt->storageType);
        ASSERT_EQ(expected.usedForWriting, dataIt->usedForWriting);

        assertUrls(expected.url, dataIt->url, Credentials::EncryptionMode::asteriks);
    }

    void thenResourceShouldHaveUrlUnencrypted(const nx::vms::api::StorageData& expected)
    {
        const auto storage = storageByName(expected.name);

        const auto u1 = nx::utils::Url(expected.url);
        const auto u2 = nx::utils::Url(storage->getUrl());

        ASSERT_EQ(u1.toString(QUrl::RemoveUserInfo), u2.toString(QUrl::RemoveUserInfo));
        ASSERT_EQ(u1.userName(), u2.userName());
        ASSERT_EQ(u1.password(), u2.password());
    }

    nx::vms::api::StorageData storageDataWithUrl(const QString& url)
    {
        nx::vms::api::StorageData result;
        result.isBackup = false;
        result.addParams = generateRandomParams(10);
        result.spaceLimit = 100;
        result.storageType = "test";
        result.usedForWriting = false;
        result.url = url;
        result.name = "test storage";
        result.parentId = m_server.commonModule()->moduleGUID();

        return result;
    }

    void whenServerStopped()
    {
        ASSERT_TRUE(m_server.stop());
    }

    void whenServerStarted()
    {
        ASSERT_TRUE(m_server.start());
    }

    void initializeDbConnection()
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", kDbConnectionName);
        m_db.setDatabaseName(serverDbFilePath());
        ASSERT_TRUE(m_db.open());
    }

    void whenStorageMigrationRecordRemoved()
    {
        assertMigrationExists();
        auto query = createAndExecQuery(
            "delete from south_migrationhistory where migration = ?", kMigrationFileName);
    }

    void whenStorageUrlInDbAltered(const nx::vms::api::StorageData& data)
    {

    }

private:
    struct Credentials
    {
        enum class EncryptionMode
        {
            hexEncrypted,
            off,
            asteriks,
        };

        QString username;
        QString password;

        Credentials() = default;
        Credentials(const nx::utils::Url& url):
            username(url.userName()),
            password(url.password())
        {}
    };

    static constexpr const char* const kDbConnectionName = "test";
    static constexpr const char* const kMigrationFileName =
        ":/updates/100_10172019_encrypt_storage_url_credentials.sql";

    MediaServerLauncher m_server = MediaServerLauncher(
        QString(), 0,
        {MediaServerLauncher::DisabledFeature::noPlugins,
            MediaServerLauncher::DisabledFeature::noMonitorStatistics,
            MediaServerLauncher::DisabledFeature::noResourceDiscovery});
    QSqlDatabase m_db;

    void assertUrls(
        const QString& expected, const QString& actual, Credentials::EncryptionMode encryption)
    {
        if (expected.isEmpty())
        {
            ASSERT_TRUE(actual.isEmpty());
            return;
        }

        const auto expectedUrl = nx::utils::Url(expected);
        const auto actualUrl = nx::utils::Url(actual);

        if (!expectedUrl.isValid())
        {
            ASSERT_FALSE(actualUrl.isValid());
            return;
        }

        assertCredentials(Credentials(expectedUrl), Credentials(actualUrl), encryption);
    }

    void assertCredentials(
        const Credentials& expected,
        const Credentials& actual,
        Credentials::EncryptionMode encryption)
    {
        switch (encryption)
        {
            case Credentials::EncryptionMode::off:
                ASSERT_EQ(expected.username, actual.username);
                ASSERT_EQ(expected.password, actual.password);
                break;
            case Credentials::EncryptionMode::hexEncrypted:
                using namespace nx::utils;
                ASSERT_EQ(encodeHexStringFromStringAES128CBC(expected.username), actual.username);
                ASSERT_EQ(encodeHexStringFromStringAES128CBC(expected.password), actual.password);
                break;
            case Credentials::EncryptionMode::asteriks:
                ASSERT_EQ(ec2::kHiddenPasswordFiller, actual.username);
                ASSERT_EQ(ec2::kHiddenPasswordFiller, actual.password);
                break;
        }
    }

    QString serverDbFilePath() const
    {
        return closeDirPath(m_server.dataDir()) + "ecs.sqlite";
    }

    QnStorageResourcePtr storageByName(const QString& name)
    {
        const auto resourcePool = m_server.serverModule()->resourcePool();
        QnStorageResourcePtr storage;

        while (!storage)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            const auto resources = resourcePool->getResources();
            auto it = std::find_if(
                resources.cbegin(), resources.cend(),
                [name](const auto& resource) { return resource->getName() == name; });

            if (it == resources.cend())
                continue;

            storage = (*it).dynamicCast<QnStorageResource>();
        }

        return storage;
    }

    template<typename... Args>
    QSqlQuery createAndExecQuery(const QString& queryString, Args&&... args)
    {
        QSqlQuery query(m_db);
        [&]() { ASSERT_TRUE(query.prepare(queryString)) << queryString.toStdString(); }();
        std::initializer_list<int> dummy = {(query.addBindValue(std::forward<Args>(args)), 42)...};
        (void) dummy;

        [&]() { ASSERT_TRUE(query.exec()); }();
        return query;
    }

    void assertMigrationExists()
    {
        auto query = createAndExecQuery("SELECT migration from south_migrationhistory");
        while (query.next())
        {
            if (query.value(0).toString() == kMigrationFileName)
                return;
        }

        ASSERT_TRUE(false) << "Migration not found";
    }
};

TEST_P(Storage, CredentialsEncryption)
{
    const auto data = storageDataWithUrl(GetParam().url);
    whenStorageDataSaved(data);
    thenItShouldBeRetrievableViaApiWithEncryptedCredentials(data);
    thenResourceShouldHaveUrlUnencrypted(data);
}

TEST_P(Storage, Migration)
{
    auto data = storageDataWithUrl(GetParam().url);
    whenStorageDataSaved(data);
    whenServerStopped();
    initializeDbConnection();
    whenStorageMigrationRecordRemoved();
    whenStorageUrlInDbAltered(data);
    whenServerStarted();
    thenItShouldBeRetrievableViaApiWithEncryptedCredentials(data);
    thenResourceShouldHaveUrlUnencrypted(data);
}

INSTANTIATE_TEST_CASE_P(Storage_SaveGet_differentUrls,
    Storage,
    ::testing::Values(
        Param(""),
        Param("test://user:password@host/some/path"),
        Param("test://:password@host/some/path"),
        Param("test://user:@host/some/path"),
        Param("test://u:p@host/some/path"),
        Param("test://:p@host/some/path"),
        Param("test://u:@host/some/path"),
        Param("test://host/some/path")
        ));

} // namespace test
} // namespace nx
