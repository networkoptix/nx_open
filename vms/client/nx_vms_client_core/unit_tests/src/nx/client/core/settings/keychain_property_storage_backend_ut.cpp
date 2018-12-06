#include <gtest/gtest.h>

#include <nx/client/core/settings/keychain_property_storage_backend.h>
#include <nx/vms/client/core/common/utils/encoded_credentials.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/app_info.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {
namespace test {

namespace {

using nx::utils::AppInfo;
using StringHash = QHash<QString, QString>;

static const QString kServiceName(
    AppInfo::organizationName() + " " + AppInfo::productNameLong() + " Test");

class KeychainBackendTestStorage:
    public QObject,
    public nx::utils::property_storage::Storage
{
    using base_type = nx::utils::property_storage::Storage;

public:
    KeychainBackendTestStorage():
        base_type(new KeychainBackend(kServiceName))
    {
        load();
    }

    virtual ~KeychainBackendTestStorage() override
    {
        for (const auto property: properties())
            removeValue(property->name);
    }

    void reload()
    {
        load();
    }

    KeychainBackend* backend() const
    {
        return static_cast<KeychainBackend*>(base_type::backend());
    }

    using SystemAuthenticationDataHash = QHash<QnUuid, QList<EncodedCredentials>>;

    Property<QString> string{
        this, "string"};

    Property<EncodedCredentials> encodedCredentials{
        this, "encodedCredentials"};

    Property<StringHash> stringHash{
        this, "stringHash"};

    Property<QStringList> stringList{
        this, "stringList"};

    // System credentials by local system id.
    Property<SystemAuthenticationDataHash> systemAuthenticationData{
        this, "systemAuthenticationData"};


};

} // namespace

class KeychainBackendStorageTest: public testing::Test
{
protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        settings.reset(new KeychainBackendTestStorage());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        settings.reset();
    }

    QScopedPointer<KeychainBackendTestStorage> settings;
};

TEST_F(KeychainBackendStorageTest, string)
{
    static const QString kTestValue("test");
    settings->string = kTestValue;
    settings->reload();
    ASSERT_EQ(kTestValue, settings->string());

    static const QString kExpectedKeyName = settings->string.name;
    ASSERT_EQ(
        settings->backend()->readValue(kExpectedKeyName),
        QJson::serialized(kTestValue));
}

TEST_F(KeychainBackendStorageTest, encodedCredentials)
{
    static const EncodedCredentials kTestValue("user", "password", {});
    settings->encodedCredentials = kTestValue;
    settings->reload();
    ASSERT_EQ(kTestValue, settings->encodedCredentials());

    static const QString kExpectedKeyName = settings->encodedCredentials.name;
    ASSERT_EQ(
        settings->backend()->readValue(kExpectedKeyName),
        QJson::serialized(kTestValue));
}

TEST_F(KeychainBackendStorageTest, stringHash)
{
    static const StringHash kTestValue{{"key1", "value1"}, {"key2", "value2"}};
    settings->stringHash = kTestValue;
    settings->reload();
    ASSERT_EQ(kTestValue, settings->stringHash());

    static const QString kExpectedKeyName = settings->stringHash.name;
    ASSERT_EQ(
        settings->backend()->readValue(kExpectedKeyName),
        QJson::serialized(kTestValue));
}

TEST_F(KeychainBackendStorageTest, stringList)
{
    static const QStringList kTestValue{"value1", "value2"};
    settings->stringList = kTestValue;
    settings->reload();
    ASSERT_EQ(kTestValue, settings->stringList());

    static const QString kExpectedKeyName = settings->stringList.name;
    ASSERT_EQ(
        settings->backend()->readValue(kExpectedKeyName),
        QJson::serialized(kTestValue));
}

} // namespace test
} // namespace nx::vms::client::core
