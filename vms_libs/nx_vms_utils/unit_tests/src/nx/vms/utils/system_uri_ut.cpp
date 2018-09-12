#include <gtest/gtest.h>

#include <nx/vms/utils/system_uri.h>
#include <nx/vms/utils/app_info.h>

using namespace nx::vms::utils;

/*
    Valid Links Examples

    Http(s) Universal Links

        http://cloud-demo.hdw.mx/client - just open the client
        http://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh - open the client and connect to cloud
        http://cloud-demo.hdw.mx/client/localhost:7001?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh

   Native Links
        nx-vms://cloud-demo.hdw.mx/ - just open the client
        nx-vms://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh - open the client and login to cloud

        nx-vms://cloud-demo.hdw.mx/client - just open the client
        nx-vms://cloud-demo.hdw.mx/client/localhost:7001?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud
        nx-vms://cloud-demo.hdw.mx/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud

        nx-vms://cloud-demo.hdw.mx/systems - just open the client
        nx-vms://cloud-demo.hdw.mx/systems/localhost:7001?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/systems/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud
        nx-vms://cloud-demo.hdw.mx/systems/localhost:7001/view?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/systems/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud
*/


namespace
{
    const QString kCloudDomain("cloud-demo.hdw.mx");
    const QString kLocalDomain("localhost:7001");

    const QString kCloudSystemId("d0b73d03-3e2e-405d-8226-019c83b13a08");
    const QString kLocalSystemId("localhost:7001");
    const QString kEncodedLocalSystemId("localhost%3A7001");
    const QString kInvalidSystemId("_invalid_system_id_");

    const QString kEmptyUser("");
    const QString kUser("user");
    const QString kPassword("password");
    const QString kEncodedAuthKey("dXNlcjpwYXNzd29yZA%3D%3D");  /**< Url-encoded key for 'user:password' string in base64 encoding. */

    const QString kParamKey("key");
    const QString kParamValue("value");
}


namespace nx {
namespace vms {
namespace utils {

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

void PrintTo(const SystemUri& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

} // namespace utils
} // namespace vms
} // namespace nx


class SystemUriTest : public testing::Test
{
public:
    void validateLink(const QString& link)
    {
        SystemUri parsed(link);
        ASSERT_FALSE(parsed.isNull());  /*< Uri will be null if url was invalid.*/
        assertEqual(m_uri, parsed);
    }

    void validateToString(const QString& expected)
    {
        ASSERT_TRUE(m_uri.isValid());
        ASSERT_EQ(expected, m_uri.toString());
        ASSERT_EQ(expected, m_uri.toUrl().toString());
    }

    void assertEqual(const SystemUri& l, const SystemUri& r)
    {
        ASSERT_EQ(l.protocol(), r.protocol());
        ASSERT_EQ(l.scope(), r.scope());
        ASSERT_EQ(l.clientCommand(), r.clientCommand());
        ASSERT_EQ(l.domain(), r.domain());
        ASSERT_EQ(l.systemId(), r.systemId());
        ASSERT_EQ(l.authenticator().user, r.authenticator().user);
        ASSERT_EQ(l.authenticator().password, r.authenticator().password);
        ASSERT_EQ(l.referral().source, r.referral().source);
        ASSERT_EQ(l.referral().context, r.referral().context);
        ASSERT_EQ(l.rawParameters(), r.rawParameters());
        ASSERT_EQ(l, r);
    }

protected:
    SystemUri m_uri;
};

/* Null and not-null tests. */

TEST_F( SystemUriTest, GenericNull )
{
    ASSERT_TRUE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullCommand)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullDomain)
{
    m_uri.setDomain(kCloudDomain);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullProtocol)
{
    /* Generic protocol for null uri is http. */
    ASSERT_EQ(SystemUri::Protocol::Http, m_uri.protocol());
    m_uri.setProtocol(SystemUri::Protocol::Https);
    ASSERT_FALSE(m_uri.isNull());
    m_uri.setProtocol(SystemUri::Protocol::Native);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullSystemId)
{
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullParameters)
{
    SystemUri::Parameters custom;
    custom["customParameter"] = "customValue";
    m_uri.setRawParameters(custom);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullAuth)
{
    m_uri.setAuthenticator(kUser, QString());
    ASSERT_FALSE(m_uri.isNull());

    m_uri.setAuthenticator(QString(), kPassword);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullReferral)
{
    m_uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::None);
    ASSERT_FALSE(m_uri.isNull());

    m_uri.setReferral(SystemUri::ReferralSource::None, SystemUri::ReferralContext::SetupWizard);
    ASSERT_FALSE(m_uri.isNull());
}

/** Validness tests */

TEST_F(SystemUriTest, GenericInvalid)
{
    ASSERT_FALSE(m_uri.isValid());
}

/** Empty client command. */
TEST_F(SystemUriTest, genericClientCommandInvalid)
{
    m_uri.setDomain(kCloudDomain);
    ASSERT_FALSE(m_uri.isValid());
}

/** Generic client command. Only domain is required. */
TEST_F(SystemUriTest, genericClientValid)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kCloudDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic cloud command. Domain is required. */
TEST_F(SystemUriTest, genericCloudValidDomain)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kCloudDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic cloud command. Only password is not acceptable. */
TEST_F(SystemUriTest, genericCloudValidAuth)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setDomain(kCloudDomain);
    ASSERT_TRUE(m_uri.isValid());

    m_uri.setAuthenticator(kEmptyUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());
}

/** Generic system command. Domain is required. */
TEST_F(SystemUriTest, genericSystemValidDomain)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kCloudDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic system command. Only password is not acceptable if system id is present. */
TEST_F(SystemUriTest, genericSystemValidAuth)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());

    m_uri.setAuthenticator(kEmptyUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());
}

/** Generic system command. System Id is required if auth is present. */
TEST_F(SystemUriTest, genericSystemValidSystemId)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kInvalidSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Works only with native protocol. */
TEST_F(SystemUriTest, directSystemValidProtocol)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setProtocol(SystemUri::Protocol::Https);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setProtocol(SystemUri::Protocol::Native);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Any system id is required. */
TEST_F(SystemUriTest, directSystemAnySystemId)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kInvalidSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Valid system id is required if auth is provided. */
TEST_F(SystemUriTest, directSystemValidSystemIdWithAuth)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kInvalidSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());

    m_uri.setSystemId(kCloudSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Valid client commands for native protocol without auth. */
TEST_F(SystemUriTest, directSystemNativeClient)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setClientCommand(SystemUri::ClientCommand::OpenOnPortal);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Login to cloud for native protocol. */
TEST_F(SystemUriTest, directSystemNativeCloud)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    ASSERT_FALSE(m_uri.isValid());
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}

/* Testing links parsing */

// http://cloud-demo.hdw.mx/client - just open the client
// https://cloud-demo.hdw.mx/client - just open the client
TEST_F(SystemUriTest, universalLinkClient)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);

    ASSERT_EQ(SystemUri::Protocol::Http, m_uri.protocol());
    validateLink(QString("http://%1/client").arg(kCloudDomain));

    m_uri.setProtocol(SystemUri::Protocol::Https);
    validateLink(QString("https://%1/client").arg(kCloudDomain));
}

// http://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh - open the client and connect to cloud
// https://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh - open the client and connect to cloud
TEST_F(SystemUriTest, universalLinkCloud)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setDomain(kCloudDomain);
    m_uri.setAuthenticator(kUser, kPassword);

    validateLink(QString("http://%1/cloud?auth=%2").arg(kCloudDomain).arg(kEncodedAuthKey));

    m_uri.setProtocol(SystemUri::Protocol::Https);
    validateLink(QString("https://%1/cloud?auth=%2").arg(kCloudDomain).arg(kEncodedAuthKey));
}

// http://cloud-demo.hdw.mx/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// http://cloud-demo.hdw.mx/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
// https://cloud-demo.hdw.mx/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// https://cloud-demo.hdw.mx/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, universalLinkLocalSystem)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);

    validateLink(QString("http://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));

    m_uri.setProtocol(SystemUri::Protocol::Https);
    validateLink(QString("https://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("https://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
}

// http://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
// http://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh
// https://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
// https://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, universalLinkCloudSystem)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);

    validateLink(QString("http://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));

    m_uri.setProtocol(SystemUri::Protocol::Https);
    validateLink(QString("https://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("https://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
}

// nx-vms://cloud-demo.hdw.mx/ - just open the client
// nx-vms://cloud-demo.hdw.mx/client - just open the client
// nx-vms://cloud-demo.hdw.mx/systems - just open the client
TEST_F(SystemUriTest, nativeLinkClient)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);

    validateLink(QString("nx-vms://%1").arg(kCloudDomain));
    validateLink(QString("nx-vms://%1/client").arg(kCloudDomain));
    validateLink(QString("nx-vms://%1/systems").arg(kCloudDomain));
}

// nx-vms://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh - open the client and login to cloud
TEST_F(SystemUriTest, nativeLinkCloud)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setAuthenticator(kUser, kPassword);

    validateLink(QString("nx-vms://%1/cloud?auth=%2").arg(kCloudDomain).arg(kEncodedAuthKey));
}

// nx-vms://cloud-demo.hdw.mx/client/localhost:7001?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/client/localhost:7001/view?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/systems/localhost:7001?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/systems/localhost:7001/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, nativeLinkLocalSystem)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);

    validateLink(QString("nx-vms://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/systems/%2?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/systems/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
}

// nx-vms://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/client/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/systems/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
// nx-vms://cloud-demo.hdw.mx/systems/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh
TEST_F(SystemUriTest, nativeLinkCloudSystem)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);

    validateLink(QString("nx-vms://%1/client/%2?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/client/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/systems/%2?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("nx-vms://%1/systems/%2/view?auth=%3")
        .arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
}

/* Testing invalid domains. */

TEST_F(SystemUriTest, directLinkCloudSystem)
{
    m_uri.setDomain(kCloudSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);

    validateLink(QString("http://%1/client?auth=%2").arg(kCloudSystemId).arg(kEncodedAuthKey));
    ASSERT_FALSE(m_uri.isValid());
}

/* Referral links */

TEST_F(SystemUriTest, referralLinkSource)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::None);
    validateLink(QString("http://%1/?from=client").arg(kCloudDomain));
    validateLink(QString("http://%1/?FrOm=ClIeNt").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::MobileClient, SystemUri::ReferralContext::None);
    validateLink(QString("http://%1/?from=mobile").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::CloudPortal, SystemUri::ReferralContext::None);
    validateLink(QString("http://%1/?from=portal").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::WebAdmin, SystemUri::ReferralContext::None);
    validateLink(QString("http://%1/?from=webadmin").arg(kCloudDomain));
}

TEST_F(SystemUriTest, referralLinkContext)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setReferral(SystemUri::ReferralSource::None, SystemUri::ReferralContext::SetupWizard);
    validateLink(QString("http://%1/?context=setup").arg(kCloudDomain));
    validateLink(QString("http://%1/?CoNtExT=SeTuP").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::None, SystemUri::ReferralContext::SettingsDialog);
    validateLink(QString("http://%1/?context=settings").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::None, SystemUri::ReferralContext::WelcomePage);
    validateLink(QString("http://%1/?context=startpage").arg(kCloudDomain));

    m_uri.setReferral(SystemUri::ReferralSource::None, SystemUri::ReferralContext::CloudMenu);
    validateLink(QString("http://%1/?context=menu").arg(kCloudDomain));
}


/* Testing toUrl() / toString methods. */

TEST_F(SystemUriTest, genericClientToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    validateToString(QString("http://%1/client").arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericClientLocalDomainToString)
{
    m_uri.setDomain(kLocalDomain); /*Make sure port is parsed ok.*/
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    validateToString(QString("http://%1/client").arg(kLocalDomain));
}

TEST_F(SystemUriTest, genericClientToStringNative)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    validateToString(QString("%1://%2/client").arg(AppInfo::nativeUriProtocol()).arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericCloudToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setAuthenticator(kUser, kPassword);
    validateToString(QString("http://%1/cloud?auth=%2").arg(kCloudDomain).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, genericSystemCloudSystemIdToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);
    validateToString(QString("http://%1/client/%2/view?auth=%3").arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, genericSystemLocalSystemIdToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);
    validateToString(QString("http://%1/client/%2/view?auth=%3").arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, directSystemCloudSystemIdToString)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);
    validateToString(QString("%1://%2/client/view?auth=%3").arg(AppInfo::nativeUriProtocol()).arg(kCloudSystemId).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, customParametersToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.addParameter(kParamKey, kParamValue);
    validateToString(QString("http://%1/client?%2=%3").arg(kCloudDomain).arg(kParamKey).arg(kParamValue));
}

TEST_F(SystemUriTest, referralLinkToString)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::WelcomePage);
    validateToString(QString("http://%1/client?from=%2&context=%3")
                     .arg(kCloudDomain)
                     .arg(SystemUri::toString(SystemUri::ReferralSource::DesktopClient))
                     .arg(SystemUri::toString(SystemUri::ReferralContext::WelcomePage))
                          );
}

TEST_F(SystemUriTest, constructors)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::WelcomePage);
    m_uri.addParameter(kParamKey, kParamValue);

    SystemUri copy(m_uri);
    assertEqual(m_uri, copy);

    SystemUri assigned = m_uri;
    assertEqual(m_uri, assigned);
}

TEST_F(SystemUriTest, assignmentOperator)
{
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setReferral(SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::WelcomePage);
    m_uri.addParameter(kParamKey, kParamValue);

    SystemUri copy;
    copy = SystemUri(m_uri.toString());
    assertEqual(m_uri, copy);
}

TEST_F(SystemUriTest, openCameraAtTimestampSupport)
{
    static const QString kResourceId("ed93120e-0f50-3cdf-39c8-dd52a640688c");
    static const qint64 kTimestamp(1520110800000);

    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setResourceIds({QnUuid::fromStringSafe(kResourceId)});
    m_uri.setTimestamp(kTimestamp);

    // Note: auth parameter must be the last as it contain %3.
    validateLink(QString("nx-vms://%1/client/%2/view/?resources=%3&timestamp=%4&auth=%5")
        .arg(kCloudDomain)
        .arg(kLocalSystemId)
        .arg(kResourceId)
        .arg(kTimestamp)
        .arg(kEncodedAuthKey)
    );
}

TEST_F(SystemUriTest, openTwoCameras)
{
    static const QString kResourceId1("ed93120e-0f50-3cdf-39c8-dd52a640688c");
    static const QString kResourceId2("04762367-751f-2727-25ca-9ae0dc6727de");

    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setResourceIds({QnUuid::fromStringSafe(kResourceId1),
        QnUuid::fromStringSafe(kResourceId2)});

    // Note: auth parameter must be the last as it contain %3.
    validateLink(QString("nx-vms://%1/client/%2/view/?resources=%3:%4&auth=%5")
        .arg(kCloudDomain)
        .arg(kLocalSystemId)
        .arg(kResourceId1)
        .arg(kResourceId2)
        .arg(kEncodedAuthKey)
    );
}