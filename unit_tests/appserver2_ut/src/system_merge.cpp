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
#include <transaction/transaction.h>

namespace ec2 {
namespace test {

namespace {

class MediaServerClientEx:
    public MediaServerClient
{
    using base_type = MediaServerClient;

public:
    MediaServerClientEx(const QUrl& baseRequestUrl):
        base_type(baseRequestUrl)
    {
    }

    void ec2GetTransactionLog(
        std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)> completionHandler)
    {
        performAsyncEc2Call("ec2/getTransactionLog", std::move(completionHandler));
    }

    ec2::ErrorCode ec2GetTransactionLog(ec2::ApiTransactionDataList* result)
    {
        using Ec2GetTransactionLogAsyncFuncPointer =
            void(MediaServerClientEx::*)(
                std::function<void(ec2::ErrorCode, ec2::ApiTransactionDataList)>);

        return syncCallWrapper(
            this,
            static_cast<Ec2GetTransactionLogAsyncFuncPointer>(
                &MediaServerClientEx::ec2GetTransactionLog),
            result);
    }
};

} // namespace

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

    QnRestResult::Error mergeTo(const PeerHelper& remotePeer)
    {
        MergeSystemData mergeSystemData;
        mergeSystemData.takeRemoteSettings = true;
        mergeSystemData.mergeOneServer = false;
        mergeSystemData.ignoreIncompatible = false;
        const auto nonce = nx::utils::generateRandomName(7);
        mergeSystemData.getKey = buildAuthKey("/api/mergeSystems", m_ownerCredentials, nonce);
        mergeSystemData.postKey = buildAuthKey("/api/mergeSystems", m_ownerCredentials, nonce);
        mergeSystemData.url = nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(remotePeer.endpoint()).toString();

        auto mediaServerClient = prepareMediaServerClient();
        return mediaServerClient->mergeSystems(mergeSystemData).error;
    }

    ec2::ErrorCode getTransactionLog(ec2::ApiTransactionDataList* result)
    {
        auto mediaServerClient = prepareMediaServerClient();
        return mediaServerClient->ec2GetTransactionLog(result);
    }

    SocketAddress endpoint() const
    {
        return m_process.moduleInstance()->endpoint();
    }

private:
    QString m_dataDir;
    nx::utils::test::ModuleLauncher<Appserver2ProcessPublic> m_process;
    QString m_systemName;
    nx_http::Credentials m_ownerCredentials;

    std::unique_ptr<MediaServerClientEx> prepareMediaServerClient()
    {
        auto mediaServerClient = std::make_unique<MediaServerClientEx>(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(m_process.moduleInstance()->endpoint()));
        mediaServerClient->setUserCredentials(m_ownerCredentials);
        return mediaServerClient;
    }

    QString buildAuthKey(
        const nx::String& url,
        const nx_http::Credentials& credentials,
        const nx::String& nonce)
    {
        const auto ha1 = nx_http::calcHa1(
            credentials.username,
            nx::network::AppInfo::realm(),
            credentials.authToken.value);
        const auto ha2 = nx_http::calcHa2(nx_http::Method::get, url);
        const auto response = nx_http::calcResponse(ha1, nonce, ha2);
        return lm("%1:%2:%3").args(credentials.username, nonce, response).toUtf8().toBase64();
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
        m_prevResult = m_servers.back().peer->mergeTo(*m_servers.front().peer);
    }

    void thenMergeSucceeded()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, m_prevResult);
    }
    
    void thenAllServersSynchronizedData()
    {
        for (;;)
        {
            std::vector<::ec2::ApiTransactionDataList> transactionLogs;
            for (auto& server: m_servers)
            {
                ::ec2::ApiTransactionDataList transactionLog;
                ASSERT_EQ(::ec2::ErrorCode::ok, server.peer->getTransactionLog(&transactionLog));
                transactionLogs.push_back(std::move(transactionLog));
            }

            const ::ec2::ApiTransactionDataList* prevTransactionLog = nullptr;
            bool allLogsAreEqual = true;
            for (const auto& transactionLog: transactionLogs)
            {
                if (prevTransactionLog)
                {
                    if (*prevTransactionLog != transactionLog)
                        allLogsAreEqual = false;
                }

                prevTransactionLog = &transactionLog;
            }

            if (allLogsAreEqual)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    struct ServerContext
    {
        std::unique_ptr<PeerHelper> peer;
    };

    QString m_tmpDir;
    std::vector<ServerContext> m_servers;
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
