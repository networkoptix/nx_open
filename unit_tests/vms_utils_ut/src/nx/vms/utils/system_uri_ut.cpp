#include <gtest/gtest.h>

#include <nx/vms/utils/system_uri.h>

using namespace nx::vms::utils;

/*
    Valid Links Examples

    Http(s) Generic Links
        http://cloud-demo.hdw.mx/
        http://cloud-demo.hdw.mx/client
        http://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/system/localhost:7001?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/system/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/system/localhost:7001/view?auth=YWJyYTprYWRhYnJh
        http://cloud-demo.hdw.mx/system/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh

    Native Generic Links
        nx-vms://cloud-demo.hdw.mx/
        nx-vms://cloud-demo.hdw.mx/client
        nx-vms://cloud-demo.hdw.mx/cloud?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/system/localhost:7001?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/system/d0b73d03-3e2e-405d-8226-019c83b13a08?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud
        nx-vms://cloud-demo.hdw.mx/system/localhost:7001/view?auth=YWJyYTprYWRhYnJh
        nx-vms://cloud-demo.hdw.mx/system/d0b73d03-3e2e-405d-8226-019c83b13a08/view?auth=YWJyYTprYWRhYnJh  - auth will be used to login to cloud

    Http(s) Direct Links
        http://localhost:7001/ - cannot be distinguished from generic link
        http://localhost:7001/system?auth=YWJyYTprYWRhYnJh
        http://localhost:7001/system/view?auth=YWJyYTprYWRhYnJh

    Native Direct Links
        nx-vms://localhost:7001/ - cannot be distinguished from generic link
        nx-vms://localhost:7001/client - cannot be distinguished from generic link
        nx-vms://localhost:7001/cloud?auth=YWJyYTprYWRhYnJh - cannot be distinguished from generic link
        nx-vms://localhost:7001/system?auth=YWJyYTprYWRhYnJh - auth will be used to login directly
        nx-vms://d0b73d03-3e2e-405d-8226-019c83b13a08/system?auth=YWJyYTprYWRhYnJh - auth will be used to login to cloud
*/


namespace
{
    const QString kCloudDomain("cloud-demo.hdw.mx");
    const QString kLocalDomain("localhost:7001");

    const QString kCloudSystemId("d0b73d03-3e2e-405d-8226-019c83b13a08");
    const QString kLocalSystemId("localhost:7001");
    const QString kEncodedLocalSystemId("localhost%3A7001");

    const QString kUser("user");
    const QString kPassword("password");
    const QString kEncodedAuthKey("dXNlcjpwYXNzd29yZA%3D%3D");  /**< Url-encoded key for 'user:password' string in base64 encoding. */
}

namespace nx
{
    namespace vms
    {
        namespace utils
        {
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

            void PrintTo(const SystemUri& val, ::std::ostream* os)
            {
                *os << val.toString().toStdString();
            }
        }
    }
}



class SystemUriTest : public testing::Test
{
public:
    void validateLink(const QString& link)
    {
        SystemUri parsed(link);
        ASSERT_FALSE(parsed.isNull());  /*< Uri will be null if url was invalid.*/
        assertEqual(m_uri, parsed);
    }

    void assertEqual(const SystemUri& l, const SystemUri& r)
    {
        ASSERT_EQ(l.protocol(), r.protocol());
        ASSERT_EQ(l.clientCommand(), r.clientCommand());
        ASSERT_EQ(l.domain(), r.domain());
        ASSERT_EQ(l.systemId(), r.systemId());
        ASSERT_EQ(l.authenticator().user, r.authenticator().user);
        ASSERT_EQ(l.authenticator().password, r.authenticator().password);
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

/** Generic cloud command. Auth is required. */
TEST_F(SystemUriTest, genericCloudValidAuth)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setDomain(kCloudDomain);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic system command. Domain is required. */
TEST_F(SystemUriTest, genericSystemValidDomain)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kCloudDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic system command. Auth is required. */
TEST_F(SystemUriTest, genericSystemValidAuth)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setDomain(kCloudDomain);
    m_uri.setSystemId(kLocalSystemId);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}

/** Generic system command. System Id is required. */
TEST_F(SystemUriTest, genericSystemValidSystemId)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setDomain(kCloudDomain);
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. System Id is required. */
TEST_F(SystemUriTest, directSystemValidSystemId)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. System Id is required. */
TEST_F(SystemUriTest, directSystemValidClient)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setSystemId(kLocalSystemId);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Valid client commands for http protocol. */
TEST_F(SystemUriTest, directSystemValidClientCommandHttp)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    ASSERT_EQ(SystemUri::Protocol::Http, m_uri.protocol());
    m_uri.setSystemId(kLocalSystemId);
    /* Cannot connect to cloud with direct http link. */
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    ASSERT_FALSE(m_uri.isValid());
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Valid client commands for https protocol. */
TEST_F(SystemUriTest, directSystemValidClientCommandHttps)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Https);
    m_uri.setSystemId(kLocalSystemId);
    /* Cannot connect to cloud with direct http link. */
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    ASSERT_FALSE(m_uri.isValid());
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Valid client commands for native protocol. */
TEST_F(SystemUriTest, directSystemNativeClient)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setSystemId(kLocalSystemId);
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

/** Direct system command. Login to system for native protocol. */
TEST_F(SystemUriTest, directSystemNativeSystem)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setSystemId(kLocalSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_FALSE(m_uri.isValid());
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}

/** Direct system command. Login to cloud system for native protocol. */
TEST_F(SystemUriTest, directCloudNativeSystem)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setSystemId(kCloudSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    ASSERT_FALSE(m_uri.isValid());
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}

/* Testing links */
TEST_F(SystemUriTest, genericLinkClientShort)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    validateLink(QString("http://%1/").arg(kCloudDomain));
    validateLink(QString("http://%1/client").arg(kCloudDomain));
    validateLink(QString("http://%1/client/").arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericLinkClientProtocolHttp)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);

    ASSERT_EQ(SystemUri::Protocol::Http, m_uri.protocol());
    validateLink(QString("http://%1/").arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericLinkClientProtocolHttps)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    m_uri.setProtocol(SystemUri::Protocol::Https);
    validateLink(QString("https://%1/").arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericLinkClientProtocolNative)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    m_uri.setDomain(kCloudDomain);
    //TODO: #GDM customizable
    m_uri.setProtocol(SystemUri::Protocol::Native);
    validateLink(QString("nx-vms://%1/").arg(kCloudDomain));
    validateLink(QString("customprotocol://%1/").arg(kCloudDomain));
}

TEST_F(SystemUriTest, genericLinkCloud)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setAuthenticator(kUser, kPassword);

    validateLink(QString("http://%1/cloud?auth=%2").arg(kCloudDomain).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, genericLinkLocalSystem)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);

    validateLink(QString("http://%1/system/%2?auth=%3").arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/system/%2/view?auth=%3").arg(kCloudDomain).arg(kLocalSystemId).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, genericLinkCloudSystem)
{
    m_uri.setDomain(kCloudDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);

    validateLink(QString("http://%1/system/%2?auth=%3").arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/system/%2/view?auth=%3").arg(kCloudDomain).arg(kCloudSystemId).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, directLinkLocalSystem)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setDomain(kLocalDomain);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kLocalSystemId);

    validateLink(QString("http://%1/system?auth=%2").arg(kLocalDomain).arg(kEncodedAuthKey));
    validateLink(QString("http://%1/system/view?auth=%2").arg(kLocalDomain).arg(kEncodedAuthKey));
}

TEST_F(SystemUriTest, directLinkCloudSystemNative)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setProtocol(SystemUri::Protocol::Native);
    m_uri.setDomain(kCloudSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);

    validateLink(QString("nx-vms://%1/system?auth=%2").arg(kCloudSystemId).arg(kEncodedAuthKey));
}

/* Testing invalid domains */

TEST_F(SystemUriTest, directLinkCloudSystem)
{
    m_uri.setScope(SystemUri::Scope::Direct);
    m_uri.setDomain(kCloudSystemId);
    m_uri.setClientCommand(SystemUri::ClientCommand::ConnectToSystem);
    m_uri.setAuthenticator(kUser, kPassword);
    m_uri.setSystemId(kCloudSystemId);

    validateLink(QString("http://%1/system?auth=%2").arg(kCloudSystemId).arg(kEncodedAuthKey));
    ASSERT_FALSE(m_uri.isValid());
}
