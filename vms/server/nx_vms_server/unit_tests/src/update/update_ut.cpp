#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <transaction/message_bus_adapter.h>
#include <api/test_api_requests.h>
#include <nx/update/update_information.h>
#include <nx/network/http/test_http_server.h>
#include <quazip/quazipfile.h>
#include <rest/server/json_rest_handler.h>

namespace nx::vms::server::test {

static const QString kUpdateFilesPathPostfix = "/test_update_files/";

using namespace nx::test;

class Updates: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testHttpServer.bindAndListen());
    }

    void givenConnectedPeers(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            m_peers.emplace_back(std::make_unique<MediaServerLauncher>(
                QString(), 0,
                MediaServerLauncher::DisabledFeatures(
                    MediaServerLauncher::noResourceDiscovery
                    | MediaServerLauncher::noMonitorStatistics)));

            m_peers.back()->addSetting("--override-version", "4.0.0.0");
            ASSERT_TRUE(m_peers.back()->start());
        }

        for (int i = 0; i < count - 1; ++i)
        {
            const auto& to = m_peers[i + 1];
            const auto toUrl = utils::url::parseUrlFields(to->endpoint().toString(),
                network::http::kUrlSchemeName);

            m_peers[i]->serverModule()->ec2Connection()->messageBus()->addOutgoingConnectionToPeer(
                to->commonModule()->moduleGUID(), vms::api::PeerType::server, toUrl);
        }
    }

    void whenCorrectUpdateInformationSet()
    {
        prepareCorrectUpdateInformation();
        QByteArray serializedUpdateInfo;
        NX_TEST_API_POST(m_peers[0].get(), "/ec2/startUpdate", m_updateInformation);
    }

    void thenItShouldBeRetrivable()
    {
        update::Information receivedUpdateInfo;
        NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateInformation", &receivedUpdateInfo);
        ASSERT_EQ(m_updateInformation, receivedUpdateInfo);

        QList<nx::update::Status> updateStatusList;
        while (true)
        {
            NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateStatus", &updateStatusList);
            if (updateStatusList.isEmpty()
                || updateStatusList[0].code != update::Status::Code::readyToInstall)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            else
            {
                break;
            }
        }
    }

    void thenInstallUpdateWithoutPeersParameterShouldFail()
    {
        QByteArray responseBuffer;
        NX_TEST_API_POST(m_peers[0].get(), "/api/installUpdate", "", nullptr,
            network::http::StatusCode::ok, "admin", "admin", &responseBuffer);
        QnRestResult installResponse;
        QJson::deserialize(responseBuffer, &installResponse);
        ASSERT_EQ(QnRestResult::MissingParameter, installResponse.error);
    }

private:
    struct ZipContext
    {
        QString dataFileName;
        QByteArray data;

        ZipContext(const QString& dataFileName, const QByteArray& data):
            dataFileName(dataFileName),
            data(data)
        {}
    };

    std::vector<std::unique_ptr<MediaServerLauncher>> m_peers;
    nx::update::Information m_updateInformation;
    network::http::TestHttpServer m_testHttpServer;

    void prepareCorrectUpdateInformation()
    {
        m_updateInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        m_updateInformation.version = "4.0.0.1";
        m_updateInformation.packages.append(preparePackage(m_peers[0]->dataDir()));
    }

    update::Package preparePackage(const QString& dataDir)
    {
        update::Package result;
        result.arch = QnAppInfo::applicationArch();
        result.component = "server";

        QString updateFileFullPath;
        result.file = createUpdateZip(dataDir, &result.md5, &result.size, &updateFileFullPath);
        result.platform = QnAppInfo::applicationPlatform();
        result.url = serveUpdateFile(updateFileFullPath);
        result.variant = QnAppInfo::applicationPlatformModification();
        result.variantVersion = QnAppInfo::currentSystemInformation().runtimeOsVersion();

        return result;
    }

    QString serveUpdateFile(const QString& updateFilePath)
    {
        const QString path = "/update.zip";
        m_testHttpServer.registerFileProvider(path, updateFilePath, "application/zip");
        return QString("http://127.0.0.1:%1").arg(m_testHttpServer.server().address().port) + path;
    }

    static QString createUpdateZip(const QString& dataDir, QString* outMd5, qint64* outSize,
        QString* outUpdateFileFullPath)
    {
        const auto updateFilesPath = dataDir + kUpdateFilesPathPostfix;
        QDir().mkpath(updateFilesPath);
        const QByteArray updateJsonContent =
            QString(
R"JSON({
    "version" : "4.0.0.1",
    "platform" : "%1",
    "arch" : "%2",
    "modification" : "%3",
    "cloudHost" : "%4",
    "executable" : "install"
}
)JSON"
            ).arg(QnAppInfo::applicationPlatform())
                .arg(QnAppInfo::applicationArch())
                .arg(QnAppInfo::applicationPlatformModification())
                .arg(nx::network::SocketGlobals::cloud().cloudHost()).toLocal8Bit();

        const QString zipFileName = "update.zip";
        const QString zipFilePath = updateFilesPath + zipFileName;

        QList<ZipContext> zipContextList = {
            ZipContext("update.json", updateJsonContent),
            ZipContext("install", "hello")
        };

        writeFilesToZip(zipFilePath, zipContextList);
        getZipMetaData(zipFilePath, outMd5, outSize);
        *outUpdateFileFullPath = zipFilePath;

        return zipFileName;
    }

    static void getZipMetaData(const QString& zipFilePath,  QString* outMd5, qint64* outSize)
    {
        QFile file(zipFilePath);
        assert(file.open(QIODevice::ReadOnly));
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(&file);
        *outMd5 = hash.result().toHex();
        *outSize = file.size();
    }

    static void writeFilesToZip(const QString& zipFilePath, const QList<ZipContext>& zipContextList)
    {
        QuaZip zip(zipFilePath);
        zip.open(QuaZip::Mode::mdCreate);
        QuaZipFile zipFile(&zip);

        for (const auto& zipContext: zipContextList)
        {
            QuaZipNewInfo info(zipContext.dataFileName);
            zipFile.open(QIODevice::WriteOnly, info);
            zipFile.write(zipContext.data);
            zipFile.close();
        }
    }
};

TEST_F(Updates, updateInformation_onePeer_correctlySet)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationSet();
    thenItShouldBeRetrivable();
}

TEST_F(Updates, installUpdate_wontWorkWithoutPeersParameter)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationSet();
    thenItShouldBeRetrivable();
    thenInstallUpdateWithoutPeersParameterShouldFail();
}

} // namespace nx::vms::server::test
