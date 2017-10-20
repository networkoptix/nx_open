#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/network/http/auth_tools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/std/cpp14.h>

#include <api/mediaserver_client.h>
#include <test_support/appserver2_process.h>

namespace ec2 {
namespace test {

class PeerHelper
{
public:
    PeerHelper(const QString& dataDir):
        m_dataDir(dataDir)
    {
        m_ownerCredentials.username = "admin";
        m_ownerCredentials.authToken = nx_http::PasswordAuthToken("admin");
    }

    bool startAndWaitUntilStarted()
    {
        const QString dbFileArg = lm("--dbFile=%1/db.sqlite").args(m_dataDir);
        m_process.addArg(dbFileArg.toStdString().c_str());
        return m_process.startAndWaitUntilStarted();
    }

    bool configureAsLocalSystem()
    {
        auto mediaServerClient = prepareMediaServerClient();

        const QString password = nx::utils::generateRandomName(7);

        SetupLocalSystemData request;
        request.systemName = nx::utils::generateRandomName(7);
        request.password = password;

        if (mediaServerClient->setupLocalSystem(std::move(request)).error != 
                QnJsonRestResult::NoError)
        {
            return false;
        }

        m_ownerCredentials.authToken = nx_http::PasswordAuthToken(password);
        return true;
    }

private:
    QString m_dataDir;
    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic> m_process;
    QString m_systemName;
    nx_http::Credentials m_ownerCredentials;

    std::unique_ptr<MediaServerClient> prepareMediaServerClient()
    {
        auto mediaServerClient = std::make_unique<MediaServerClient>(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_process.moduleInstance()->endpoint()));
        mediaServerClient->setUserCredentials(m_ownerCredentials);
        return mediaServerClient;
    }
};

//-------------------------------------------------------------------------------------------------

class SystemMerge:
    public ::testing::Test
{
public:
    SystemMerge()
    {
        m_tmpDir =
            (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
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
            m_servers.push_back(ServerContext());
            m_servers.back().peer = 
                std::make_unique<PeerHelper>(lm("%1/peer_%2").args(m_tmpDir, i));
            ASSERT_TRUE(m_servers.back().peer->startAndWaitUntilStarted());
            ASSERT_TRUE(m_servers.back().peer->configureAsLocalSystem());
        }
    }

    void whenMergeSystems()
    {
        // TODO
    }

    void thenMergeSucceeded()
    {
        // TODO
    }
    
    void thenAllServersSynchronizedData()
    {
        // TODO
    }

private:
    struct ServerContext
    {
        std::unique_ptr<PeerHelper> peer;
    };

    QString m_tmpDir;
    std::vector<ServerContext> m_servers;

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
