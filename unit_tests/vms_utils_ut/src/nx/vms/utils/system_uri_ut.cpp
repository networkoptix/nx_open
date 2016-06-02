#include <gtest/gtest.h>

#include <nx/vms/utils/system_uri.h>

using namespace nx::vms::utils;

namespace
{
    const QString kDefaultDomain("cloud-demo.hdw.mx");
    const QString kLocalSystemId("localhost:7001");
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

/** Initial test. Check if default uri is null. */
TEST_F( SystemUriTest, init )
{
    ASSERT_TRUE(m_uri.isNull());
}

/* Not-null tests. */
TEST_F(SystemUriTest, notNullCommand)
{
    m_uri.setClientCommand(SystemUri::ClientCommand::Open);
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
