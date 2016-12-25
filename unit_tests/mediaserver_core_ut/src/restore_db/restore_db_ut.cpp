#include <memory>
#include <utility>
#include <media_server_process.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>
#include <database/db_manager.h>

#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#define GTEST_HAS_POSIX_RE 0
#include <gtest/gtest.h>


namespace detail {
class TestSystemNameProxy : public aux::SystemNameProxy
{
public:
    static const QString kConfigValue;

    virtual void loadFromConfig() override
    {
        m_systemName = kConfigValue;
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

private:
    QString m_systemName;
};

const QString TestSystemNameProxy::kConfigValue = lit("SYSTEM_NAME_CONFIG_VALUE");

class TestSettingsProxy : public aux::SettingsProxy
{
public:
    TestSettingsProxy() :
        m_cloudInstanceChanged(false),
        m_connectedToCloud(false)
    {}

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

class BaseRestoreDbTest : public testing::Test
{
    QByteArray kHash;
    QByteArray kDigest;
    QByteArray kCryptHash;
    QByteArray kRealm;

    QString kSystemName;
    QnUuid kLocalSystemId;

public:
    virtual void SetUp() override
    {
        kHash = QByteArray("HASH");
        kDigest = QByteArray("DIGEST");
        kCryptHash = QByteArray("CRYPT_HASH");
        kRealm = QByteArray("REALM");

        kSystemName = lit("SYSTEM_NAME");
        kLocalSystemId = QnUuid::createUuid();

        settingsProxy.reset(new detail::TestSettingsProxy);

        resourcePool = std::unique_ptr<QnResourcePool>(new QnResourcePool);
        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);

        admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
        mediaServer = QnMediaServerResourcePtr(new QnMediaServerResource);
    }

    void fillDefaultAdminAuth()
    {
        admin->setHash(kHash);
        admin->setDigest(kDigest);
        admin->setCryptSha512Hash(kCryptHash);
        admin->setRealm(kRealm);
    }

    void setDefaultSystemIdentity()
    {
        settingsProxy->setSystemName(kSystemName);
        settingsProxy->setLocalSystemId(kLocalSystemId);
    }

    void shutdownBeforeRestore()
    {
        restoreData = aux::savePersistentDataBeforeDbRestore(admin, mediaServer, settingsProxy.get());
    }

    void setConnectedToCloud()
    {
        settingsProxy->setCloudInstanceChanged(false);
        settingsProxy->setConnectedToCloud(true);
    }

    void assertNoAdminAuthDataHasBeenSaved()
    {
        ASSERT_TRUE(restoreData.cryptSha512Hash.isNull());
        ASSERT_TRUE(restoreData.digest.isNull());
        ASSERT_TRUE(restoreData.hash.isNull());
        ASSERT_TRUE(restoreData.realm.isNull());
    }

    void assertAdminAuthDataHasBeenSaved()
    {
        ASSERT_EQ(restoreData.cryptSha512Hash, kCryptHash);
        ASSERT_EQ(restoreData.digest, kDigest);
        ASSERT_EQ(restoreData.hash, kHash);
        ASSERT_EQ(restoreData.realm, kRealm);
    }

    void assertAdminAuthDataHasBeenRestored()
    {
        ASSERT_EQ(admin->getCryptSha512Hash(), restoreData.cryptSha512Hash);
        ASSERT_EQ(restoreData.digest, admin->getDigest());
        ASSERT_EQ(restoreData.hash, admin->getHash());
        ASSERT_EQ(restoreData.realm, admin->getRealm());
    }

    void assertLocalSystemNameAndIdHaveBeenSaved()
    {
        ASSERT_EQ(restoreData.localSystemId, kLocalSystemId.toByteArray());
        ASSERT_EQ(restoreData.localSystemName, kSystemName);
    }

    void restartServer()
    {
        settingsProxy.reset(new detail::TestSettingsProxy);
        systemNameProxy.reset(new detail::TestSystemNameProxy);
        admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
    }

    void useRestoreData()
    {
        ec2::aux::applyRestoreDbData(restoreData, admin);
        aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
    }

    void assertAdminIsDisabled()
    {
        ASSERT_EQ(admin->isEnabled(), false);
    }

    void assertAdminIsEnabled()
    {
        ASSERT_EQ(admin->isEnabled(), true);
    }

    void assertLocalSystemIdAndSystemNameHaveBeenRestored()
    {
        ASSERT_EQ(restoreData.localSystemId, settingsProxy->localSystemId().toByteArray());
        ASSERT_EQ(restoreData.localSystemName, settingsProxy->systemName());
    }

    void assertNeedToResetSystem(bool isNewServerInstance, bool expectedValue)
    {
        ASSERT_EQ(aux::needToResetSystem(isNewServerInstance, settingsProxy.get()), expectedValue);
    }

    QnUserResourcePtr admin;
    QnMediaServerResourcePtr mediaServer;
    BeforeRestoreDbData restoreData;
    TestSystemNameProxyPtr systemNameProxy;
    TestSettingsProxyPtr settingsProxy;

    std::unique_ptr<QnResourcePool> resourcePool;
    std::unique_ptr<QnCommonModule> commonModule;
};

class BackupWhenCloudAndRestoreWhenCloud : public BaseRestoreDbTest
{
protected:
    virtual void SetUp() override
    {
        BaseRestoreDbTest::SetUp();
        admin->setEnabled(false);
        setConnectedToCloud();
        fillDefaultAdminAuth();
        setDefaultSystemIdentity();
    }
};

TEST_F(BackupWhenCloudAndRestoreWhenCloud, main)
{
    shutdownBeforeRestore();
    restartServer();
    setConnectedToCloud();
    useRestoreData();

    assertNoAdminAuthDataHasBeenSaved();
    assertLocalSystemIdAndSystemNameHaveBeenRestored();
    assertNeedToResetSystem(false, false);
}

class BackupWhenLocalAndRestoreWhenLocal : public BaseRestoreDbTest
{
protected:
    virtual void SetUp()
    {
        BaseRestoreDbTest::SetUp();
        fillDefaultAdminAuth();
        setDefaultSystemIdentity();
    }
};

TEST_F(BackupWhenLocalAndRestoreWhenLocal, main)
{
    assertAdminIsEnabled();

    shutdownBeforeRestore();
    restartServer();
    assertAdminAuthDataHasBeenSaved();

    useRestoreData();

    assertAdminIsEnabled();
    assertAdminAuthDataHasBeenRestored();
    assertLocalSystemIdAndSystemNameHaveBeenRestored();
    assertNeedToResetSystem(false, false);
}

class BackupWhenLocalAndRestoreWhenCloud : public BaseRestoreDbTest
{
protected:
    virtual void SetUp() override
    {
        BaseRestoreDbTest::SetUp();
        fillDefaultAdminAuth();
        setDefaultSystemIdentity();
    }
};

TEST_F(BackupWhenLocalAndRestoreWhenCloud, main)
{
    setConnectedToCloud();

    shutdownBeforeRestore();
    assertAdminAuthDataHasBeenSaved();
    restartServer();
    setConnectedToCloud();
    useRestoreData();

    assertAdminIsEnabled();
    assertAdminAuthDataHasBeenRestored();
    assertLocalSystemIdAndSystemNameHaveBeenRestored();
    assertNeedToResetSystem(false, false);
}

class BackupWhenCloudAndRestoreWhenLocal : public BaseRestoreDbTest
{
protected:
    virtual void SetUp() override
    {
        BaseRestoreDbTest::SetUp();
        setDefaultSystemIdentity();
        fillDefaultAdminAuth();
    }
};

TEST_F(BackupWhenCloudAndRestoreWhenLocal, main)
{
    shutdownBeforeRestore();
    restartServer();
    useRestoreData();

    assertAdminIsEnabled();
    assertAdminAuthDataHasBeenRestored();
    assertLocalSystemIdAndSystemNameHaveBeenRestored();
    assertNeedToResetSystem(false, false);
}

class CleanStart : public BaseRestoreDbTest {};

TEST_F(CleanStart, main)
{
    assertNoAdminAuthDataHasBeenSaved();
    assertAdminIsEnabled();
    assertNeedToResetSystem(true, true);
}

class CloudInstanceChanged : public BaseRestoreDbTest
{
protected:
    virtual void SetUp() override
    {
        BaseRestoreDbTest::SetUp();
        settingsProxy->setCloudInstanceChanged(true);
        settingsProxy->setConnectedToCloud(true);
        admin->setEnabled(false);
        setDefaultSystemIdentity();
    }
};

TEST_F(CloudInstanceChanged, main)
{
    assertNoAdminAuthDataHasBeenSaved();
    assertAdminIsDisabled();
    assertNeedToResetSystem(true, true);
}
