#include <memory>
#include <media_server_process.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>


namespace detail {
class TestSystemNameProxy : public aux::SystemNameProxy
{
public:
    virtual void loadFromConfig() override
    {
        m_systemName = m_configValue;
    }

    virtual void clearFromConfig() override
    {
        m_systemName.clear();
    }

    virtual void resetToDefault() override
    {
        m_systemName = lit("DEFAULT");
    }

    virtual bool isDefault() const override
    {
        return m_systemName == lit("DEFAULT");
    }

    virtual QString value() const override
    {
        return m_systemName;
    }

    void setConfigValue(const QString& value)
    {
        m_configValue = value;
    }

private:
    QString m_systemName;
    QString m_configValue;
};

class TestSettingsProxy : public aux::SettingsProxy
{
public:
    virtual QString systemName() const override
    {
        return m_systemName;
    }

    virtual QnUuid localSystemId() const override
    {
        return m_localSystemId;
    }

    virtual void setSystemName(const QString& systemName) override
    {
        m_systemName = systemName;
    }

    virtual void setLocalSystemId(const QnUuid& localSystemId) override
    {
        m_localSystemId = localSystemId;
    }

    virtual bool isCloudInstanceChanged() const override
    {
        return m_cloudInstanceChanged;
    }

    virtual bool isConnectedToCloud() const override
    {
        return m_connectedToCloud;
    }

    void setCloudInstanceChanged(bool value)
    {
        m_cloudInstanceChanged = value;
    }

    void setConnectedToCloud(bool value)
    {
        m_connectedToCloud = value;
    }

private:
    QString m_systemName;
    QnUuid m_localSystemId;
    bool m_cloudInstanceChanged;
    bool m_connectedToCloud;
};
}

using TestSystemNameProxyPtr = std::unique_ptr<detail::TestSystemNameProxy>;
using TestSettingsProxyPtr = std::unique_ptr<detail::TestSettingsProxy>;

class RestoreDbTest : public testing::Test
{
    QByteArray kHash;
    QByteArray kDigest;
    QByteArray kCryptHash;
    QByteArray kRealm;

    QString kSystemName;
    QnUuid kLocalSystemId;

protected:
    void fillAdminAuth()
    {
        admin->setHash(kHash);
        admin->setDigest(kDigest);
        admin->setCryptSha512Hash(kCryptHash);
        admin->setRealm(kRealm);
    }

    virtual void SetUp() override
    {
        kHash = QByteArray("HASH");
        kDigest = QByteArray("DIGEST");
        kCryptHash = QByteArray("CRYPT_HASH");
        kRealm = QByteArray("REALM");

        kSystemName = lit("SYSTEM_NAME");
        kLocalSystemId = QnUuid::createUuid();

        settingsProxy.reset(new detail::TestSettingsProxy);
        systemNameProxy.reset(new detail::TestSystemNameProxy);

        resourcePool = std::unique_ptr<QnResourcePool>(new QnResourcePool);
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);

        admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
        mediaServer = QnMediaServerResourcePtr(new QnMediaServerResource);
    }

    void whenBackupIsPerformedWhileConnectedToCloudAndNowConnected()
    {
        settingsProxy->setCloudInstanceChanged(false);
        settingsProxy->setConnectedToCloud(true);
        settingsProxy->setSystemName(kSystemName);
        settingsProxy->setLocalSystemId(kLocalSystemId);

        admin->setEnabled(false);
        fillAdminAuth();
        restoreData = aux::savePersistentDataBeforeDbRestore(admin, mediaServer, settingsProxy.get());
    }

    void thenNoUserAuthDataShouldHasBeenSaved()
    {
        ASSERT_TRUE(restoreData.cryptSha512Hash.isNull());
        ASSERT_TRUE(restoreData.digest.isNull());
        ASSERT_TRUE(restoreData.hash.isNull());
        ASSERT_TRUE(restoreData.realm.isNull());
    }

    void thenLocalSystemNameAndIdShouldHaveBeenSaved()
    {
        ASSERT_EQ(restoreData.localSystemId, kLocalSystemId.toByteArray());
        ASSERT_EQ(restoreData.localSystemName, kSystemName);
    }

    QnUserResourcePtr admin;
    QnMediaServerResourcePtr mediaServer;
    BeforeRestoreDbData restoreData;
    TestSystemNameProxyPtr systemNameProxy;
    TestSettingsProxyPtr settingsProxy;

    std::unique_ptr<QnResourcePool> resourcePool;
    std::unique_ptr<QnCommonModule> commonModule;
};

TEST_F(RestoreDbTest, CleanStartNoRestore)
{
    whenBackupIsPerformedWhileConnectedToCloudAndNowConnected();
    thenNoUserAuthDataShouldHasBeenSaved();
    thenLocalSystemNameAndIdShouldHaveBeenSaved();
}

