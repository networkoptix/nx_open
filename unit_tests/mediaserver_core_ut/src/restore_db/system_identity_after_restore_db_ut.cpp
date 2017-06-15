#include <memory>
#include <utility>
#include <media_server_process.h>
#include <media_server_process_aux.h>
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
class TestSystemNameProxy : public nx::mserver_aux::SystemNameProxy
{
public:
    enum class SystemNameInConfig
    {
        yes,
        no
    };

    TestSystemNameProxy(SystemNameInConfig value) : m_isSystemNameInConfig(value) {}

    static const QString kConfigValue;
    static const QString kDefaultValue;

    virtual void loadFromConfig() override
    {
        if (m_isSystemNameInConfig == SystemNameInConfig::yes)
            m_systemName = kConfigValue;
    }

    virtual void clearFromConfig() override
    {
        m_systemName.clear();
    }

    virtual void resetToDefault() override
    {
        m_systemName = kDefaultValue;
    }

    virtual bool isDefault() const override
    {
        return m_systemName == kDefaultValue;
    }

    virtual QString value() const override
    {
        return m_systemName;
    }

private:
    QString m_systemName;
    SystemNameInConfig m_isSystemNameInConfig;
};

const QString TestSystemNameProxy::kConfigValue = lit("SYSTEM_NAME_CONFIG_VALUE");
const QString TestSystemNameProxy::kDefaultValue = lit("DEFAULT_SYSTEM_NAME_VALUE");

class TestSettingsProxy : public nx::mserver_aux::SettingsProxy
{
public:
    TestSettingsProxy() :
        m_cloudInstanceChanged(false),
        m_connectedToCloud(false),
        m_isSystemIdFromSystemName(false)
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

    virtual bool isSystemIdFromSystemName() const override
    {
        return m_isSystemIdFromSystemName;
    }

    virtual QString getMaxServerKey() const override
    {
        return lit("SERVER_KEY");
    }

    void setIsSystemIdFromSystemName(bool value)
    {
        m_isSystemIdFromSystemName = value;
    }

private:
    QString m_systemName;
    QnUuid m_localSystemId;
    bool m_cloudInstanceChanged;
    bool m_connectedToCloud;
    bool m_isSystemIdFromSystemName;
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

public:
    QString kSystemName;
    QnUuid kLocalSystemId;

    virtual void SetUp() override
    {
        kHash = QByteArray("HASH");
        kDigest = QByteArray("DIGEST");
        kCryptHash = QByteArray("CRYPT_HASH");
        kRealm = QByteArray("REALM");

        kSystemName = lit("SYSTEM_NAME");
        kLocalSystemId = QnUuid::createUuid();

        settingsProxy.reset(new detail::TestSettingsProxy);

        commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule(/*isClient*/false,
            nx::core::access::Mode::direct));

        admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
        mediaServer = QnMediaServerResourcePtr(new QnMediaServerResource(commonModule.get()));
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
        restoreData = nx::mserver_aux::savePersistentDataBeforeDbRestore(admin, mediaServer, settingsProxy.get());
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

    void restartServer(detail::TestSystemNameProxy::SystemNameInConfig value)
    {
        settingsProxy.reset(new detail::TestSettingsProxy);
        systemNameProxy.reset(new detail::TestSystemNameProxy(value));
        admin = QnUserResourcePtr(new QnUserResource(QnUserType::Local));
    }

    void useRestoreData()
    {
        ec2::aux::applyRestoreDbData(restoreData, admin);
        nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
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
        ASSERT_EQ(nx::mserver_aux::needToResetSystem(isNewServerInstance, settingsProxy.get()), expectedValue);
    }

    QnUserResourcePtr admin;
    QnMediaServerResourcePtr mediaServer;
    BeforeRestoreDbData restoreData;
    TestSystemNameProxyPtr systemNameProxy;
    TestSettingsProxyPtr settingsProxy;

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
    restartServer(detail::TestSystemNameProxy::SystemNameInConfig::yes);
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
    restartServer(detail::TestSystemNameProxy::SystemNameInConfig::yes);
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
    restartServer(detail::TestSystemNameProxy::SystemNameInConfig::yes);
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
    restartServer(detail::TestSystemNameProxy::SystemNameInConfig::yes);
    useRestoreData();

    assertAdminIsEnabled();
    assertAdminAuthDataHasBeenRestored();
    assertLocalSystemIdAndSystemNameHaveBeenRestored();
    assertNeedToResetSystem(false, false);
}

class CleanStart : public BaseRestoreDbTest {};

TEST_F(CleanStart, main)
{
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::no));
    assertNoAdminAuthDataHasBeenSaved();
    assertAdminIsEnabled();
    assertNeedToResetSystem(true, true);

    useRestoreData();
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
    assertNeedToResetSystem(false, true);
}

TEST_F(BaseRestoreDbTest, SetUpSystemIdentity_SystemIdAndNameExistInDb)
{
    settingsProxy->setSystemName(kSystemName);
    settingsProxy->setLocalSystemId(kLocalSystemId);
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::no));
    nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));

    ASSERT_EQ(settingsProxy->systemName(), kSystemName);
    ASSERT_EQ(settingsProxy->localSystemId(), kLocalSystemId);
}

TEST_F(BaseRestoreDbTest, SetUpSystemIdentity_NoSystemNameAndIdInDb_SystemNameInConfig_NoIdFromNameFlag)
{
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::yes));
    nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
    ASSERT_EQ(settingsProxy->systemName(), detail::TestSystemNameProxy::kConfigValue);
    ASSERT_EQ(settingsProxy->localSystemId(),
              guidFromArbitraryData(detail::TestSystemNameProxy::kConfigValue +
                  settingsProxy->getMaxServerKey()));
}

TEST_F(BaseRestoreDbTest, SetUpSystemIdentity_NoSystemNameAndIdInDb_SystemNameInConfig_IdFromNameFlagSet)
{
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::yes));
    settingsProxy->setIsSystemIdFromSystemName(true);
    nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
    ASSERT_EQ(settingsProxy->systemName(), detail::TestSystemNameProxy::kConfigValue);
    ASSERT_EQ(settingsProxy->localSystemId(),
              guidFromArbitraryData(detail::TestSystemNameProxy::kConfigValue));
}

TEST_F(BaseRestoreDbTest, SetUpSystemIdentity_NoSystemNameAndIdInDb_NoSystemNameInConfig_IdFromNameFlagSet)
{
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::no));
    settingsProxy->setIsSystemIdFromSystemName(true);
    nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
    ASSERT_EQ(settingsProxy->systemName(), detail::TestSystemNameProxy::kDefaultValue);
    ASSERT_EQ(settingsProxy->localSystemId(), guidFromArbitraryData(detail::TestSystemNameProxy::kDefaultValue));
}

TEST_F(BaseRestoreDbTest, SetUpSystemIdentity_NoSystemNameAndIdInDb_NoSystemNameInConfig_NoIdFromNameFlagSet)
{
    systemNameProxy.reset(new detail::TestSystemNameProxy(detail::TestSystemNameProxy::SystemNameInConfig::no));
    settingsProxy->setIsSystemIdFromSystemName(false);
    nx::mserver_aux::setUpSystemIdentity(restoreData, settingsProxy.get(), std::move(systemNameProxy));
    ASSERT_EQ(settingsProxy->systemName(), detail::TestSystemNameProxy::kDefaultValue);
    ASSERT_EQ(settingsProxy->localSystemId(), QnUuid());
}


TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOn_RestoreDataIsNotSet_NoServerInDb)
{
    bool noSetupWizard = true;
    bool serverFoundInDb = false;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOff_RestoreDataIsNotSet_NoServerInDb)
{
    bool noSetupWizard = false;
    bool serverFoundInDb = false;
    ASSERT_TRUE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOn_RestoreDataIsNotSet_ServerInDb)
{
    bool noSetupWizard = true;
    bool serverFoundInDb = true;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOff_RestoreDataIsNotSet_ServerInDb)
{
    bool noSetupWizard = false;
    bool serverFoundInDb = true;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOff_RestoreDataIsSet_ServerInDb)
{
    fillDefaultAdminAuth();
    setDefaultSystemIdentity();
    shutdownBeforeRestore();

    bool noSetupWizard = false;
    bool serverFoundInDb = true;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOn_RestoreDataIsSet_ServerInDb)
{
    fillDefaultAdminAuth();
    setDefaultSystemIdentity();
    shutdownBeforeRestore();

    bool noSetupWizard = true;
    bool serverFoundInDb = true;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOn_RestoreDataIsSet_NoServerInDb)
{
    fillDefaultAdminAuth();
    setDefaultSystemIdentity();
    shutdownBeforeRestore();

    bool noSetupWizard = true;
    bool serverFoundInDb = false;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}

TEST_F(BaseRestoreDbTest, NeedToResetSystem_NoSetupWizardIsOff_RestoreDataIsSet_NoServerInDb)
{
    fillDefaultAdminAuth();
    setDefaultSystemIdentity();
    shutdownBeforeRestore();

    bool noSetupWizard = false;
    bool serverFoundInDb = false;
    ASSERT_FALSE(nx::mserver_aux::isNewServerInstance(restoreData, serverFoundInDb, noSetupWizard));
}
