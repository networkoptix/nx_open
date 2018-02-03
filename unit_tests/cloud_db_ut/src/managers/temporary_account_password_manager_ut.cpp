#include <gtest/gtest.h>

#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "base_persistent_data_test.h"

namespace nx {
namespace cdb {
namespace test {

class TemporaryAccountPasswordManager:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    TemporaryAccountPasswordManager():
        m_tempPasswordManager(
            m_settings,
            &persistentDbManager()->queryExecutor())
    {
        m_email = BusinessDataGenerator::generateRandomEmailAddress();
    }

protected:
    void generateTemporaryCredentials()
    {
        m_credentials.accountEmail = m_email;
        m_tempPasswordManager.addRandomCredentials(m_email, &m_credentials);
    }

    void assertCredentialsLoginEqualAccountEmail()
    {
        ASSERT_EQ(m_email, m_credentials.accountEmail);
    }

private:
    conf::Settings m_settings;
    cdb::TemporaryAccountPasswordManager m_tempPasswordManager;
    data::TemporaryAccountCredentials m_credentials;
    std::string m_email;
};

TEST_F(TemporaryAccountPasswordManager, temporary_login_is_equal_to_account_email)
{
    generateTemporaryCredentials();
    assertCredentialsLoginEqualAccountEmail();
}

} // namespace test
} // namespace cdb
} // namespace nx
