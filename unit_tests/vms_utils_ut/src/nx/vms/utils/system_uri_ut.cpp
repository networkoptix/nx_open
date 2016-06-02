#include <gtest/gtest.h>

#include <nx/vms/utils/system_uri.h>

using namespace nx::vms::utils;

namespace
{
    const QString kDefaultDomain("cloud-demo.hdw.mx");
    const QString kLocalSystemId("localhost:7001");
    const QString kUser("user");
    const QString kPassword("password");
}

namespace nx
{
    namespace vms
    {
        namespace utils
        {
            void PrintTo(SystemUri::Protocol val, ::std::ostream* os)
            {
                auto toString = [](SystemUri::Protocol value) -> QString
                {
                    switch (value)
                    {
                        case SystemUri::Protocol::Http:
                            return "http";
                        case SystemUri::Protocol::Https:
                            return "https";
                        case SystemUri::Protocol::Native:
                            return "nx-vms";
                        default:
                            break;
                    }
                    return QString();
                };

                *os << toString(val).toStdString();
                //*os << QnLexical::serialized(val).toStdString();
            }
        }
    }
}



class SystemUriTest : public testing::Test
{
protected:
    SystemUri m_uri;
};

/* Null and not-null tests. */

TEST_F( SystemUriTest, defaultNull )
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
    m_uri.setDomain(kDefaultDomain);
    ASSERT_FALSE(m_uri.isNull());
}

TEST_F(SystemUriTest, notNullProtocol)
{
    /* Default protocol for null uri is http. */
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

TEST_F(SystemUriTest, defaultInvalid)
{
    ASSERT_FALSE(m_uri.isValid());
}

/** Empty client command. */
TEST_F(SystemUriTest, genericClientCommandInvalid)
{
    m_uri.setDomain(kDefaultDomain);
    ASSERT_FALSE(m_uri.isValid());
}

/** Default client command. Only domain is required. */
TEST_F(SystemUriTest, genericClientValid)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Client);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kDefaultDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Default cloud command. Domain is required. */
TEST_F(SystemUriTest, genericCloudValidDomain)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setDomain(kDefaultDomain);
    ASSERT_TRUE(m_uri.isValid());
}

/** Default cloud command. Auth is required. */
TEST_F(SystemUriTest, genericCloudValidAuth)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::LoginToCloud);
    m_uri.setDomain(kDefaultDomain);
    ASSERT_FALSE(m_uri.isValid());

    m_uri.setAuthenticator(kUser, kPassword);
    ASSERT_TRUE(m_uri.isValid());
}
