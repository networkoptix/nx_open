#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/std/cpp14.h>

#include <api/mediaserver_client.h>
#include <test_support/appserver2_process.h>
#include <test_support/peer_wrapper.h>
#include <transaction/transaction.h>

namespace ec2 {
namespace test {

class SystemMerge:
    public ::testing::Test
{
public:
    SystemMerge()
    {
        m_tmpDir = (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
            ? QDir::homePath()
            : nx::utils::TestOptions::temporaryDirectoryPath()) + "/merge_test.data";
        QDir(m_tmpDir).removeRecursively();
    }

    ~SystemMerge()
    {
        QDir(m_tmpDir).removeRecursively();
    }

    static void SetUpTestCase()
    {
        s_staticCommonModule = std::make_unique<QnStaticCommonModule>(
            Qn::PeerType::PT_Server);
    }

    static void TearDownTestCase()
    {
        s_staticCommonModule.reset();
    }

protected:
    void givenTwoSingleServerSystems()
    {
        for (int i = 0; i < 2; ++i)
        {
            m_servers.emplace_back(
                std::make_unique<PeerWrapper>(lm("%1/peer_%2").args(m_tmpDir, i)));
            ASSERT_TRUE(m_servers.back()->startAndWaitUntilStarted());
            ASSERT_TRUE(m_servers.back()->configureAsLocalSystem());
        }
    }

    void whenMergeSystems()
    {
        m_prevResult = m_servers.back()->mergeTo(*m_servers.front());
    }

    void thenMergeSucceeded()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, m_prevResult);
    }
    
    void thenAllServersSynchronizedData()
    {
        for (;;)
        {
            if (PeerWrapper::areAllPeersHaveSameTransactionLog(m_servers))
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    QString m_tmpDir;
    std::vector<std::unique_ptr<PeerWrapper>> m_servers;
    QnRestResult::Error m_prevResult = QnRestResult::Error::NoError;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;
};

std::unique_ptr<QnStaticCommonModule> SystemMerge::s_staticCommonModule;

TEST_F(SystemMerge, two_systems_can_be_merged)
{
    givenTwoSingleServerSystems();

    whenMergeSystems();

    thenMergeSucceeded();
    thenAllServersSynchronizedData();
}

} // namespace test
} // namespace ec2
