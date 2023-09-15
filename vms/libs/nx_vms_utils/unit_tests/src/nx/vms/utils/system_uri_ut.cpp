// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/branding.h>
#include <nx/utils/uuid.h>
#include <nx/vms/utils/system_uri.h>

using namespace nx::vms::utils;
using namespace nx::network::http;

/**
 * Examples of the supported native links.
 *
 * Open the client:
 *  * nx-vms://nxvms.com
 *
 * Login to the Local System using Digest auth:
 *   auth=base64(login:password)
 *  * nx-vms://nxvms.com/client/10.0.3.14:7001?auth=YWJyYTprYWRhYnJh
 *  client and systems are interchangeable:
 *  * nx-vms://nxvms.com/systems/10.0.3.14:7001?auth=YWJyYTprYWRhYnJh
 *  system address can be used directly to shorten the url:
 *  * nx-vms://10.0.3.14:7001?auth=YWJyYTprYWRhYnJh
 *
 * Login to the Local System using Bearer token:
 * * nx-vms://localhost:7001/view?auth=vms-a9e6c2caf6f34942a3aa414375295d29-DvU3vLK9c7
 *
 * Login to the Cloud using short-term code:
 * * nx-vms://nxvms.com/cloud?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
 *
 * Login to the Cloud System using short-term code:
 * * nx-vms://nxvms.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
 *
 * Open several provided cameras on a given time:
 * * nx-vms://localhost:7001?auth=YWRtaW46cXdlYXNkMTIz&resources=ed93120e-0f50-3cdf-39c8-dd52a640688c:04762367-751f-2727-25ca-9ae0dc6727de&timestamp=1680868281000
 **/

namespace {

const QString kCloudHost("example.com");

const QString kCloudSystemAddress("d0b73d03-3e2e-405d-8226-019c83b13a08");
const QString kLocalSystemAddress("localhost:7001");
const QString kEncodedLocalSystemAddress("localhost%3A7001");
const QString kInvalidSystemAddress("_invalid_system_id_");

const std::string kEmptyUser("");
const std::string kUser("user");
const std::string kPassword("password");
const std::string kToken("vms-a9e6c2caf6f34942a3aa414375295d29-DvU3vLK9c7");
const std::string kTemporaryToken("vmsTmp-abcde01234");
const QString kAuthCode("nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753");
const auto kPasswordCredentials = PasswordCredentials(kUser, kPassword);
const auto kTokenCredentials = Credentials(BearerAuthToken(kToken));
const auto kTemporaryTokenCredentials = Credentials(BearerAuthToken(kTemporaryToken));

// Url-encoded key for 'user:password' string in base64 encoding.
const QString kEncodedAuthKey("dXNlcjpwYXNzd29yZA%3D%3D");

} // namespace

namespace nx::vms::utils {

void PrintTo(SystemUri::Scope val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

void PrintTo(SystemUri::Protocol val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

void PrintTo(SystemUri::ClientCommand val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

void PrintTo(SystemUri::SystemAction val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

void PrintTo(SystemUri::ReferralSource val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

void PrintTo(SystemUri::ReferralContext val, ::std::ostream* os)
{
    *os << SystemUri::toString(val).toStdString();
}

std::ostream& operator<<(std::ostream& s, const SystemUri& uri)
{
    return s << uri.toString().toStdString();
}

} // namespace nx::vms::utils

class SystemUriTest : public testing::Test
{
public:
    void validateLink(const QString& link)
    {
        SystemUri parsed(link);
        assertEqual(m_uri, parsed);
        EXPECT_TRUE(m_uri.isValid()) << m_uri;
    }

    void validateToString(const QString& expected)
    {
        EXPECT_TRUE(m_uri.isValid()) << m_uri;
        EXPECT_EQ(expected, m_uri.toString());
        EXPECT_EQ(expected, m_uri.toUrl().toString());
    }

    void assertEqual(const SystemUri& l, const SystemUri& r)
    {
        EXPECT_EQ(l.protocol, r.protocol);
        EXPECT_EQ(l.scope, r.scope);
        EXPECT_EQ(l.clientCommand, r.clientCommand);
        EXPECT_EQ(l.cloudHost, r.cloudHost);
        EXPECT_EQ(l.systemAddress, r.systemAddress);
        EXPECT_EQ(l.credentials.username, r.credentials.username);
        EXPECT_EQ(l.credentials.authToken, r.credentials.authToken);
        EXPECT_EQ(l.authCode, r.authCode);
        EXPECT_EQ(l.referral.source, r.referral.source);
        EXPECT_EQ(l.referral.context, r.referral.context);
        EXPECT_EQ(l, r);
    }

protected:
    SystemUri m_uri;
};

//-------------------------------------------------------------------------------------------------
// Validness tests

TEST_F(SystemUriTest, GenericInvalid)
{
    EXPECT_FALSE(m_uri.isValid()) << m_uri;
}

/** Empty client command. */
TEST_F(SystemUriTest, genericClientCommandInvalid)
{
    m_uri.cloudHost = kCloudHost;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;
}

/** Generic client command. Only domain is required. */
TEST_F(SystemUriTest, genericClientValid)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.cloudHost = kCloudHost;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Generic cloud command. Domain is required. */
TEST_F(SystemUriTest, genericCloudValidDomain)
{
    m_uri.clientCommand = SystemUri::ClientCommand::LoginToCloud;
    m_uri.authCode = kAuthCode;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.cloudHost = kCloudHost;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

// Login to Cloud command. Credentials are not supported anymore. Auth code shall be used instead.
TEST_F(SystemUriTest, genericCloudValidAuth)
{
    m_uri.clientCommand = SystemUri::ClientCommand::LoginToCloud;
    m_uri.cloudHost = kCloudHost;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.authCode = kAuthCode;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.credentials = kTokenCredentials;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;
}

/** Generic system command. Domain is required. */
TEST_F(SystemUriTest, genericSystemValidDomain)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    m_uri.systemAddress = kLocalSystemAddress;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.cloudHost = kCloudHost;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Generic system command. Valid credentials must be present if system id is present. */
TEST_F(SystemUriTest, genericSystemValidAuth)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kLocalSystemAddress;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.credentials = PasswordCredentials(kEmptyUser, kPassword);
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.credentials = kPasswordCredentials;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.credentials = kTokenCredentials;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Authorization type match credentials */
TEST_F(SystemUriTest, userAuthTypeMatchCredentials)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.authCode = kAuthCode;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.authCode = "";
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.userAuthType = SystemUri::UserAuthType::none;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;
}

/** Generic system command. System Id is required if auth is present. */
TEST_F(SystemUriTest, genericSystemValidSystemId)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kInvalidSystemAddress;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kLocalSystemAddress;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Direct system command. Works with both native protocol and http protocol. */
TEST_F(SystemUriTest, directSystemValidProtocol)
{
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.protocol = SystemUri::Protocol::Https;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    m_uri.protocol = SystemUri::Protocol::Native;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Direct system command. Any system address is required. */
TEST_F(SystemUriTest, directSystemAnySystemId)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kInvalidSystemAddress;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;
}

/** Direct system command. Valid system id is required if auth is provided. */
TEST_F(SystemUriTest, directSystemValidSystemIdWithAuth)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kInvalidSystemAddress;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;

    m_uri.systemAddress = kLocalSystemAddress;
    EXPECT_TRUE(m_uri.isValid()) << m_uri;

    // Password credentials are not valid for Cloud Systems.
    m_uri.systemAddress = kCloudSystemAddress;
    EXPECT_FALSE(m_uri.isValid()) << m_uri;
}

//-------------------------------------------------------------------------------------------------
// Examples tests

// Open the client:
// http://example.com
// http://example.com/client
// https://example.com
// https://example.com/client
TEST_F(SystemUriTest, universalLinkClient)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;

    EXPECT_EQ(SystemUri::Protocol::Http, m_uri.protocol);
    validateLink(QString("http://%1").arg(kCloudHost));
    validateLink(QString("http://%1/client").arg(kCloudHost));

    m_uri.protocol = SystemUri::Protocol::Https;
    validateLink(QString("https://%1").arg(kCloudHost));
    validateLink(QString("https://%1/client").arg(kCloudHost));
}

// Open the client and connect to the Cloud.
// http://example.com/cloud?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
// https://example.com/cloud?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
TEST_F(SystemUriTest, universalLinkCloud)
{
    m_uri.clientCommand = SystemUri::ClientCommand::LoginToCloud;
    m_uri.cloudHost = kCloudHost;
    m_uri.authCode = kAuthCode;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    validateLink(QString("http://%1/cloud?code=%2").arg(kCloudHost).arg(kAuthCode));

    m_uri.protocol = SystemUri::Protocol::Https;
    validateLink(QString("https://%1/cloud?code=%2").arg(kCloudHost).arg(kAuthCode));
}

// http://example.com/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// http://example.com/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
// https://example.com/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// https://example.com/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, universalLinkLocalSystem)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    validateLink(QString("http://%1/client/%2?auth=%3")
        .arg(kCloudHost).arg(kLocalSystemAddress).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/client/%2/view?auth=%3")
        .arg(kCloudHost).arg(kLocalSystemAddress).arg(kEncodedAuthKey));

    m_uri.protocol = SystemUri::Protocol::Https;
    validateLink(QString("https://%1/client/%2?auth=%3")
        .arg(kCloudHost).arg(kLocalSystemAddress).arg(kEncodedAuthKey));
    validateLink(QString("https://%1/client/%2/view?auth=%3")
        .arg(kCloudHost).arg(kLocalSystemAddress).arg(kEncodedAuthKey));
}

// http://example.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
// http://example.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
// https://example.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
// https://example.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
TEST_F(SystemUriTest, universalLinkCloudSystem)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.authCode = kAuthCode;
    m_uri.systemAddress = kCloudSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;

    validateLink(QString("http://%1/client/%2?code=%3")
        .arg(kCloudHost).arg(kCloudSystemAddress).arg(kAuthCode));
    validateLink(QString("http://%1/client/%2/view?code=%3")
        .arg(kCloudHost).arg(kCloudSystemAddress).arg(kAuthCode));

    m_uri.protocol = SystemUri::Protocol::Https;
    validateLink(QString("https://%1/client/%2?code=%3")
        .arg(kCloudHost).arg(kCloudSystemAddress).arg(kAuthCode));
    validateLink(QString("https://%1/client/%2/view?code=%3")
        .arg(kCloudHost).arg(kCloudSystemAddress).arg(kAuthCode));
}

// nx-vms://example.com/ - just open the client
// nx-vms://example.com/client - just open the client
// nx-vms://example.com/systems - just open the client
TEST_F(SystemUriTest, nativeLinkClient)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;

    validateLink(QString("%1://%2")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost));
    validateLink(QString("%1://%2/client")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost));
    validateLink(QString("%1://%2/systems")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost));
}

// Open the client and login to the Cloud.
// nx-vms://example.com/cloud?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
TEST_F(SystemUriTest, nativeLinkCloud)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::LoginToCloud;
    m_uri.authCode = kAuthCode;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;

    validateLink(QString("%1://%2/cloud?code=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kAuthCode));
}

// nx-vms://example.com/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// nx-vms://example.com/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
// nx-vms://example.com/systems/localhost:7001?auth=YWJyYTprYWRhYnJh
// nx-vms://example.com/systems/localhost:7001/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, nativeLinkLocalSystem)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    validateLink(QString("%1://%2/client/%3?auth=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
    validateLink(QString("%1://%2/client/%3/view?auth=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
    validateLink(QString("%1://%2/systems/%3?auth=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
    validateLink(QString("%1://%2/systems/%3/view?auth=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
}

// nx-vms://localhost:7001?auth=YWJyYTprYWRhYnJh
// nx-vms://localhost:7001/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, directLinkLocalSystem)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.systemAddress = kLocalSystemAddress; //< Make sure port is parsed ok.
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    validateLink(QString("%1://%2?auth=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
    validateLink(QString("%1://%2/view?auth=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
}

// nx-vms://d0b73d03-3e2e-405d-8226-019c83b13a08?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
// nx-vms://d0b73d03-3e2e-405d-8226-019c83b13a08/view?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
TEST_F(SystemUriTest, directLinkCloudSystem)
{
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.authCode = kAuthCode;
    m_uri.systemAddress = kCloudSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;

    validateLink(QString("%1://%2?code=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudSystemAddress)
        .arg(kAuthCode));
    validateLink(QString("%1://%2/view?code=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudSystemAddress)
        .arg(kAuthCode));
}


// Open the client and login to the Cloud System.
// nx-vms://nxvms.com/client/d0b73d03-3e2e-405d-8226-019c83b13a08?code=nxcdb-5e2f4fb9-654e-46e4-b0e5-8dcb4f9f3753
TEST_F(SystemUriTest, nativeLinkCloudSystem)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.authCode = kAuthCode;
    m_uri.systemAddress = kCloudSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;

    validateLink(QString("%1://%2/client/%3?code=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kCloudSystemAddress)
        .arg(kAuthCode));
}

// nx-vms://example.com/client/localhost:7001?auth=vms-a9e6c2caf6f34942a3aa414375295d29-DvU3vLK9c7
TEST_F(SystemUriTest, supportBearerTokenAuth)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kTokenCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    validateLink(QString("%1://%2/client/%3?auth=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(QString::fromStdString(kToken)));
}

// nx-vms://example.com/client/localhost:7001?tmp_token=vmsTmp-abcde01234
TEST_F(SystemUriTest, supportTemporaryBearerTokenAuth)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kTemporaryTokenCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::temporary;

    validateLink(QString("%1://%2/client/%3?tmp_token=%4")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(QString::fromStdString(kTemporaryToken)));
}

/* Referral links */

TEST_F(SystemUriTest, referralLinkSource)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    m_uri.referral.source = SystemUri::ReferralSource::DesktopClient;
    validateLink(QString("http://%1/?from=client").arg(kCloudHost));
    validateLink(QString("http://%1/?FrOm=ClIeNt").arg(kCloudHost));

    m_uri.referral.source = SystemUri::ReferralSource::MobileClient;
    validateLink(QString("http://%1/?from=mobile").arg(kCloudHost));

    m_uri.referral.source = SystemUri::ReferralSource::CloudPortal;
    validateLink(QString("http://%1/?from=portal").arg(kCloudHost));

    m_uri.referral.source = SystemUri::ReferralSource::WebAdmin;
    validateLink(QString("http://%1/?from=webadmin").arg(kCloudHost));
}

TEST_F(SystemUriTest, referralLinkContext)
{
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    m_uri.referral.context = SystemUri::ReferralContext::SetupWizard;
    validateLink(QString("http://%1/?context=setup").arg(kCloudHost));
    validateLink(QString("http://%1/?CoNtExT=SeTuP").arg(kCloudHost));

    m_uri.referral.context = SystemUri::ReferralContext::SettingsDialog;
    validateLink(QString("http://%1/?context=settings").arg(kCloudHost));

    m_uri.referral.context = SystemUri::ReferralContext::WelcomePage;
    validateLink(QString("http://%1/?context=startpage").arg(kCloudHost));

    m_uri.referral.context = SystemUri::ReferralContext::CloudMenu;
    validateLink(QString("http://%1/?context=menu").arg(kCloudHost));
}


/* Testing toUrl() / toString methods. */

TEST_F(SystemUriTest, genericClientToString)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    validateToString(QString("http://%1/client").arg(kCloudHost));
}

TEST_F(SystemUriTest, genericClientToStringNative)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.protocol = SystemUri::Protocol::Native;
    validateToString(QString("%1://%2/client")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost));
}

TEST_F(SystemUriTest, genericCloudToString)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::LoginToCloud;
    m_uri.authCode = kAuthCode;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    validateToString(QString("http://%1/cloud?code=%2").arg(kCloudHost).arg(kAuthCode));
}

TEST_F(SystemUriTest, genericSystemCloudSystemIdToString)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.authCode = kAuthCode;
    m_uri.systemAddress = kCloudSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    validateToString(QString("http://%1/client/%2/view?code=%3")
        .arg(kCloudHost).arg(kCloudSystemAddress).arg(kAuthCode));
}

TEST_F(SystemUriTest, genericSystemLocalSystemIdToString)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    validateToString(QString("http://%1/client/%2/view?auth=%3")
        .arg(kCloudHost).arg(kLocalSystemAddress).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, directClientLocalDomainToString)
{
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.systemAddress = kLocalSystemAddress; //< Make sure port is parsed ok.
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    validateToString(QString("%1://%2/view?auth=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kLocalSystemAddress)
        .arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, directSystemCloudSystemIdToString)
{
    m_uri.scope = SystemUri::Scope::direct;
    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.cloudHost = kCloudSystemAddress;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.authCode = kAuthCode;
    m_uri.systemAddress = kCloudSystemAddress;
    m_uri.userAuthType = SystemUri::UserAuthType::cloud;
    validateToString(QString("%1://%2/view?code=%3")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudSystemAddress)
        .arg(kAuthCode));
}

TEST_F(SystemUriTest, referralLinkToString)
{
    m_uri.cloudHost = kCloudHost;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.referral = {
        SystemUri::ReferralSource::DesktopClient,
        SystemUri::ReferralContext::WelcomePage
    };
    validateToString(QString("http://%1/client?from=%2&context=%3")
        .arg(kCloudHost)
        .arg(SystemUri::toString(SystemUri::ReferralSource::DesktopClient))
        .arg(SystemUri::toString(SystemUri::ReferralContext::WelcomePage)));
}

TEST_F(SystemUriTest, openCameraAtTimestampSupport)
{
    static const QString kResourceId("ed93120e-0f50-3cdf-39c8-dd52a640688c");
    static const qint64 kTimestamp(1520110800000);

    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kPasswordCredentials;
    m_uri.resourceIds = {QnUuid::fromStringSafe(kResourceId)};
    m_uri.timestamp = kTimestamp;
    m_uri.userAuthType = SystemUri::UserAuthType::local;

    // Note: auth parameter must be the last as it contain %3.
    validateLink(QString("%1://%2/client/%3/view/?resources=%4&timestamp=%5&auth=%6")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kResourceId)
        .arg(kTimestamp)
        .arg(kEncodedAuthKey)
    );
}

TEST_F(SystemUriTest, openTwoCameras)
{
    static const QString kResourceId1("ed93120e-0f50-3cdf-39c8-dd52a640688c");
    static const QString kResourceId2("04762367-751f-2727-25ca-9ae0dc6727de");

    m_uri.protocol = SystemUri::Protocol::Native;
    m_uri.clientCommand = SystemUri::ClientCommand::Client;
    m_uri.cloudHost = kCloudHost;
    m_uri.systemAddress = kLocalSystemAddress;
    m_uri.credentials = kPasswordCredentials;
    m_uri.userAuthType = SystemUri::UserAuthType::local;
    m_uri.resourceIds = {
        QnUuid::fromStringSafe(kResourceId1),
        QnUuid::fromStringSafe(kResourceId2)
    };

    // Note: auth parameter must be the last as it contain %3.
    validateLink(QString("%1://%2/client/%3/view/?resources=%4:%5&auth=%6")
        .arg(nx::branding::nativeUriProtocol())
        .arg(kCloudHost)
        .arg(kLocalSystemAddress)
        .arg(kResourceId1)
        .arg(kResourceId2)
        .arg(kEncodedAuthKey)
    );
}
