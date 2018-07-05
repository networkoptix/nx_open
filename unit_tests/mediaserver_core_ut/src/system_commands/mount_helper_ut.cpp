#include <nx/system_commands/detail/mount_helper.h>
#include <nx/system_commands.h>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>

namespace nx {
namespace system_commands {
namespace test {

enum class MountPath
{
    valid,
    invalid
};

enum class OsMount
{
    called_Ok,
    called_wrongCredentials,
    called_otherError,
    notCalled
};

enum MountCommandFlags
{
    noDomain_noVer = 0,
    domain = 1,
    defaultDomain = 2,
    ver1 = 4,
    ver2 = 8
};

enum class CredentialsFileNameCall
{
    called_Ok,
    called_Fail,
    notCalled
};

enum class GidUidCall
{
    called,
    notCalled
};

enum class UserName
{
    present_domain,
    present_noDomain,
    notPresent
};

enum class Password
{
    present,
    notPresent
};

static const std::string kValidUrl = "//host/path";
static const std::string kPath = "path";
static const uid_t kUid = 42;
static const uid_t kGid = 43;
static const std::string kCredentialsFile = "credentials";
static const std::string kUserName = "username";
static const std::string kUserNameWithDomain = "username\\DOMAIN";
static const std::string kPassword = "password";
static const std::string kDomain = "DOMAIN";
static const std::string kDefaultDomain = "WORKGROUP";
static const std::string kVer1 = "1.0";
static const std::string kVer2 = "2.0";

class TestMountHelper: public MountHelperBase
{
public:
    using MountHelperBase::MountHelperBase;

    void setDerivedExpectations(
        OsMount osMountExpectation,
        int mountCommandFlags,
        MountPath mountPathExpectation,
        CredentialsFileNameCall credentialsFileNameExpectation)
    {
        m_osMountExpectation = osMountExpectation;
        m_mountCommandFlags = mountCommandFlags;
        m_mountPathExpectation = mountPathExpectation;
        m_credentialsFileNameExpectation = credentialsFileNameExpectation;
    }

    int osMountCalledTimes() const { return m_osMountCalledTImes; }
    bool isCredentialsFileNameCalled() const { return m_credentialsFileNameCalled; }
    bool isGidCalled() const { return m_gidCalled; }
    bool isUidCalled() const { return m_uidCalled; }
    std::string username() const { return m_username; }
    std::string password() const { return m_password; }
    std::vector<std::string> domains() const { return m_domains; }

private:
    OsMount m_osMountExpectation;
    int m_mountCommandFlags;
    MountPath m_mountPathExpectation;
    CredentialsFileNameCall m_credentialsFileNameExpectation;
    int m_osMountCalledTImes = 0;
    mutable bool m_credentialsFileNameCalled = false;
    mutable bool m_gidCalled = false;
    mutable bool m_uidCalled = false;

    template<typename Expected, typename Actual>
    void assertEq(const Expected& expected, const Actual& actual) { ASSERT_EQ(expected, actual); }

    virtual SystemCommands::MountCode osMount(const std::string& command) override
    {
        m_osMountCalledTImes++;

        std::stringstream ss;
        ss << "mount -t cifs '" << kValidUrl << "' '" << kPath << "' -o uid=" << kUid
           << ",gid=" << kGid << ",credentials=" << kCredentialsFile;

        auto baseCommand = ss.str();
        assertEq(true, command.find(baseCommand) != std::string::npos);
        if (m_mountCommandFlags & MountCommandFlags::domain)
            ss << ",domain=" << kDomain;
        else if (m_mountCommandFlags & MountCommandFlags::defaultDomain)
            ss << ",domain=" << kDefaultDomain;

        if (m_mountCommandFlags & MountCommandFlags::ver1)
            ss << ",vers=" << kVer1;
        else if (m_mountCommandFlags & MountCommandFlags::ver2)
            ss << ",vers=" << kVer2;

        baseCommand = ss.str();
        if (command != baseCommand)
            return SystemCommands::MountCode::otherError;

        switch (m_osMountExpectation)
        {
        case OsMount::called_Ok: return SystemCommands::MountCode::ok;
        case OsMount::called_wrongCredentials: return SystemCommands::MountCode::wrongCredentials;
        case OsMount::called_otherError: return SystemCommands::MountCode::otherError;
        case OsMount::notCalled: assertEq(true, false); break;
        }

        return SystemCommands::MountCode::otherError;
    }

    virtual bool isMountPathAllowed(const std::string& /*path*/) const override
    {
        switch (m_mountPathExpectation)
        {
        case MountPath::invalid:
            return false;
        case MountPath::valid:
            return true;
        }
        return false;
    }

    virtual std::string credentialsFileName(
        const std::string& /*username*/,
        const std::string& /*password*/) const override
    {
        m_credentialsFileNameCalled = true;

        switch (m_credentialsFileNameExpectation)
        {
        case CredentialsFileNameCall::called_Fail:
            return "";
        case CredentialsFileNameCall::called_Ok:
            return kCredentialsFile;
        case CredentialsFileNameCall::notCalled:
            break;
        }

        return "";
    }

    virtual uid_t gid() const override
    {
        m_gidCalled = true;
        return kGid;
    }

    virtual uid_t uid() const override
    {
        m_uidCalled = true;
        return kUid;
    }
};

class MountHelper: public ::testing::Test
{
protected:
    void whenDerivedProvides(
        OsMount osMountExpectation,
        int mountCommandFlags,
        MountPath mountPathExpectation,
        CredentialsFileNameCall credentialsFileNameExpectation = CredentialsFileNameCall::called_Ok)
    {
        m_osMountExpectation = osMountExpectation;
        m_mountCommandFlags = mountCommandFlags;
        m_mountPathExpectation = mountPathExpectation;
        m_credentialsFileNameExpectation = credentialsFileNameExpectation;
    }

    void whenMountCalled(const std::string& url, const std::string& username)
    {
        m_mountHelper = std::make_unique<TestMountHelper>(username, kPassword);
        m_mountHelper->setDerivedExpectations(
            m_osMountExpectation,
            m_mountCommandFlags,
            m_mountPathExpectation,
            m_credentialsFileNameExpectation);
        m_result = m_mountHelper->mount(url, kPath);
    }

    void assertCredentialsConversion(UserName userNameExpectation, Password passwordExpectation)
    {
        boost::optional<std::string> username;
        boost::optional<std::string> password;

        switch (userNameExpectation)
        {
        case UserName::present_noDomain:
            username = kUserName;
            break;
        case UserName::present_domain:
            username = kUserNameWithDomain;
            break;
        case UserName::notPresent:
            username = "guest";
            break;
        }

        switch (passwordExpectation)
        {
        case Password::present:
            password = kPassword;
            break;
        case Password::notPresent:
            password = "";
            break;
        }

        m_mountHelper = std::make_unique<TestMountHelper>(username, password);
        if (userNameExpectation == UserName::present_domain)
        {
            ASSERT_EQ(kUserName, m_mountHelper->username());
            auto domains = m_mountHelper->domains();
            ASSERT_NE(std::find(domains.cbegin(), domains.cend(), kDomain), domains.cend());
        }
        ASSERT_EQ(password, m_mountHelper->password());
    }

    void thenMountShouldReturn(SystemCommands::MountCode mountCode)
    {
        ASSERT_EQ(m_result, mountCode);
    }

    void expectingCalls(
        int osMountCalledTimesAtLeast,
        CredentialsFileNameCall credentialsFileNameCallExpectations,
        GidUidCall gidUidCallExpectations)
    {
        ASSERT_GE(m_mountHelper->osMountCalledTimes(), osMountCalledTimesAtLeast);
        switch (credentialsFileNameCallExpectations)
        {
        case CredentialsFileNameCall::called_Ok:
        case CredentialsFileNameCall::called_Fail:
            ASSERT_TRUE(m_mountHelper->isCredentialsFileNameCalled());
            break;
        case CredentialsFileNameCall::notCalled:
            ASSERT_FALSE(m_mountHelper->isCredentialsFileNameCalled());
            break;
        }

        switch (gidUidCallExpectations)
        {
        case GidUidCall::called:
            ASSERT_TRUE(m_mountHelper->isGidCalled());
            ASSERT_TRUE(m_mountHelper->isUidCalled());
            break;
        case GidUidCall::notCalled:
            ASSERT_FALSE(m_mountHelper->isGidCalled());
            ASSERT_FALSE(m_mountHelper->isUidCalled());
            break;
        }
    }

private:
    OsMount m_osMountExpectation;
    int m_mountCommandFlags;
    MountPath m_mountPathExpectation;
    CredentialsFileNameCall m_credentialsFileNameExpectation;
    SystemCommands::MountCode m_result = SystemCommands::MountCode::otherError;
    std::unique_ptr<TestMountHelper> m_mountHelper;
};

TEST_F(MountHelper, Credentials_Conversions)
{
    assertCredentialsConversion(UserName::present_domain, Password::present);
    assertCredentialsConversion(UserName::notPresent, Password::present);
    assertCredentialsConversion(UserName::present_domain, Password::notPresent);
    assertCredentialsConversion(UserName::notPresent, Password::notPresent);
    assertCredentialsConversion(UserName::present_noDomain, Password::notPresent);
    assertCredentialsConversion(UserName::present_noDomain, Password::present);
}

TEST_F(MountHelper, success_noVer_noDomain)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::noDomain_noVer,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserName);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, fail_invalidMountPath)
{
    whenDerivedProvides(
        OsMount::called_otherError,
        MountCommandFlags::noDomain_noVer,
        MountPath::invalid);
    whenMountCalled(kValidUrl, kUserName);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 0,
        CredentialsFileNameCall::notCalled,
        GidUidCall::notCalled);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

TEST_F(MountHelper, fail_invalidUrl)
{
    whenDerivedProvides(
        OsMount::called_otherError,
        MountCommandFlags::noDomain_noVer,
        MountPath::valid);
    whenMountCalled("invalid_url", kUserName);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 0,
        CredentialsFileNameCall::notCalled,
        GidUidCall::notCalled);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

TEST_F(MountHelper, fail_invalidUsername)
{
    whenDerivedProvides(
        OsMount::called_otherError,
        MountCommandFlags::noDomain_noVer,
        MountPath::valid);
    whenMountCalled(kValidUrl, "\\adf");
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 0,
        CredentialsFileNameCall::notCalled,
        GidUidCall::notCalled);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

TEST_F(MountHelper, fail_invalidUsername_2)
{
    whenDerivedProvides(
        OsMount::called_otherError,
        MountCommandFlags::noDomain_noVer,
        MountPath::valid);
    whenMountCalled(kValidUrl, "adf\\");
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 0,
        CredentialsFileNameCall::notCalled,
        GidUidCall::notCalled);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

TEST_F(MountHelper, success_domainProvided_noVer)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::domain,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_domainProvided_ver1)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::domain | MountCommandFlags::ver1,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_domainProvided_ver2)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::domain | MountCommandFlags::ver2,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_noDomain_ver1)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::ver1,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_defaultDomain_noVer)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::defaultDomain,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_defaultDomain_ver1)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::defaultDomain | MountCommandFlags::ver1,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, success_defaultDomain_ver2)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::defaultDomain | MountCommandFlags::ver2,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::ok);
}

TEST_F(MountHelper, fail_invalidCredentials)
{
    whenDerivedProvides(
        OsMount::called_wrongCredentials,
        MountCommandFlags::defaultDomain | MountCommandFlags::ver2,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::wrongCredentials);
}

TEST_F(MountHelper, fail_osMountFail)
{
    whenDerivedProvides(
        OsMount::called_otherError,
        MountCommandFlags::defaultDomain | MountCommandFlags::ver2,
        MountPath::valid);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 1,
        CredentialsFileNameCall::called_Ok,
        GidUidCall::called);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

TEST_F(MountHelper, fail_cantCreateCredentialsFile)
{
    whenDerivedProvides(
        OsMount::called_Ok,
        MountCommandFlags::defaultDomain | MountCommandFlags::ver2,
        MountPath::valid,
        CredentialsFileNameCall::called_Fail);
    whenMountCalled(kValidUrl, kUserNameWithDomain);
    expectingCalls(
        /*osMountCalledTimesAtLeast*/ 0,
        CredentialsFileNameCall::called_Fail,
        GidUidCall::notCalled);
    thenMountShouldReturn(SystemCommands::MountCode::otherError);
}

// #TODO #akulikov test if credentialsFileName() receives correct username and password

} // namespace test
} // namespace system_commands
} // namespace nx
