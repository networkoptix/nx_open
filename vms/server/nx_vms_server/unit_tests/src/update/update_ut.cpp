#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <test_support/user_utils.h>
#include <transaction/message_bus_adapter.h>
#include <api/test_api_requests.h>
#include <api/global_settings.h>
#include <nx/update/update_information.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/app_info.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/common/utils/file_signature.h>
#include <quazip/quazipfile.h>
#include <rest/server/json_rest_handler.h>
#include <common/common_module.h>

namespace nx::vms::server::test {

namespace {

static const QString kUpdateFilesPathPostfix = "/test_update_files/";
static bool forbidden(bool value) { return value; }

static const QByteArray kPrivateKey = R"(
-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)";

const QByteArray kPublicKey = R"(
-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAxZ8xwHv7Xeb4QWOCmtpf
wTlO0EQW5aG84t8C7cqR5jiQyu0BE6nunzu9JmQA5dkvgQm5ebZ8+E6eRz2zYTGF
g+3yuUlXaFI6uVpa3hOdg5w58pCKWXlf9j+99gSBYlgFj54YK87FLoKKEitlJdIJ
mQeimLb9RTwXdK5uorZvxiOKOlW07CXbWrC+dDAcaUDDk6RkKzXNVJYCRHTWPyv3
y8WUt0ANRbN7ujSFZVWOWxqa6mh7pvCfOTMj0d1Kuk9xHWvpnng4BsYyvkHf+Kuc
bZ6W2hAhRW0tGYLj32vrztEi61FcmP7uD0kh3/Dq9QSTEWXe2cDMHpqebUb/VxpF
Vx+aoeB01WyMVXhn1InLdxqcjYkRn/iw0NWGx0FSHooF6e/LJE5GO+MFKoliiQ8A
ORTaFrGCmiyquPWdfp/fWquRCsnBHGct5RtMyj42Uapye8qQdRs/xgJEXxXvIvZ+
Z+6REWxvfLwRf8BrRN7DyGJYz3Yyxcy6FRBSxqSuzYYHVZNVDzXxGrtCqO/g4BEW
F+si/GjHJNian7OW4T2W1SNY+HWOmgFo/+tvUKHSlI+yWAVLWw4pPF5cOgCUQLgy
ethbJBjlLeEuSSliHtUUbi0M0InM92TvfnIfaWSCaVUDMNhgJpiHc9bcbuSsjnor
WB0XeRe3Ih3fhu1uE0NrWnECAwEAAQ==
-----END PUBLIC KEY-----
)";

struct UserDataWithExpectedForbiddenStatus
{
    nx::vms::api::UserDataEx userData;
    bool shouldBeForbidden = false;
};

} // namespace

using namespace nx::test;

class FtUpdates: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testHttpServer.bindAndListen());

        // Save the public key to a file.
        // Note: We can't put it directly to resources because our GitLab does not allow pem files
        // to be committed.
        m_keysDir = (nx::utils::TestOptions::temporaryDirectoryPath().isEmpty()
            ? QDir::homePath()
            : nx::utils::TestOptions::temporaryDirectoryPath()) + "/update_verification_keys";
        NX_ASSERT(QDir().mkpath(m_keysDir.absolutePath()));

        QFile keyFile(m_keysDir.absoluteFilePath("key.pem"));
        NX_ASSERT(keyFile.open(QFile::WriteOnly));
        keyFile.write(kPublicKey);
        keyFile.close();
    }

    virtual void TearDown() override
    {
        m_keysDir.removeRecursively();
    }

    void givenConnectedPeers(int count)
    {

        const QnUuid systemId = QnUuid::createUuid();
        for (int i = 0; i < count; ++i)
        {
            m_peers.emplace_back(std::make_unique<MediaServerLauncher>(QString()));

            m_peers.back()->addCmdOption("--override-version=4.0.0.0");
            m_peers.back()->addSetting("ignoreRootTool", "true");
            m_peers.back()->addSetting("additionalUpdateVerificationKeysDir",
                m_keysDir.absolutePath());
            ASSERT_TRUE(m_peers.back()->startAsync());
        }

        for (int i = 0; i < count; ++i)
        {
            ASSERT_TRUE(m_peers[i]->waitForStarted());
            m_peers[i]->commonModule()->globalSettings()->setLocalSystemId(systemId);
            m_peers[i]->commonModule()->globalSettings()->synchronizeNowSync();
        }

        connectPeers();
    }

    void whenCorrectUpdateInformationWithEmptyParticipantListSet()
    {
        setUpdateInformation();
    }

    void whenCorrectUpdateInformationWithParticipantsSet(const QList<QnUuid>& participants)
    {
        setUpdateInformation(participants);
    }

    void thenTargetUpdateInformationShouldBeRetrievable(
        bool shouldBeEmpty = false,
        const QString& versionKey = QString())
    {
        issueAndCheckUpdateInformationRequest(shouldBeEmpty, versionKey);
    }

    void thenInstalledtUpdateInformationShouldBeRetrievable(bool isEmpty)
    {
        issueAndCheckUpdateInformationRequest(isEmpty, "installed");
    }

    void thenPeersUpdateStatusShouldBe(
        const QMap<QnUuid, update::Status::Code>& expectedStatuses,
        bool isRequestLocal = false)
    {
        while (true)
        {
            QString path = "/ec2/updateStatus";
            if (isRequestLocal)
                path += "?local";

            QnJsonRestResult result;
            NX_TEST_API_GET(m_peers[0].get(), path, &result);
            const auto updateStatusList = result.deserialized<QList<nx::update::Status>>();
            bool ok = true;
            if (updateStatusList.size() == expectedStatuses.size())
            {
                for (const auto& status: updateStatusList)
                {
                    if (!expectedStatuses.contains(status.serverId)
                        || status.code != expectedStatuses[status.serverId])
                    {
                        ok = false;
                        break;
                    }
                }

                if (ok)
                    break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void thenInstallUpdateWithoutPeersParameterShouldFail()
    {
        issueInstallUpdateRequest(false, QnUuidList(), QnRestResult::MissingParameter);
    }

    QnUuid peerId(int index)
    {
        return m_peers[index]->commonModule()->moduleGUID();
    }

    void thenInstallUpdateWithPeersParameterShouldSucceed(const QList<QnUuid>& participants)
    {
        issueInstallUpdateRequest(true, participants, QnRestResult::NoError);
    }

    void thenInstallUpdateWithPeersParameterShouldFail(const QList<QnUuid>& participants)
    {
        issueInstallUpdateRequest(true, participants, QnRestResult::CantProcessRequest);
    }

    void thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest(
        const QList<QnUuid>& participants)
    {
        for (const auto& peer: m_peers)
        {
            if (participants.contains(peer->commonModule()->moduleGUID()))
                ASSERT_TRUE(peer->mediaServerProcess()->installUpdateRequestReceived());
        }
    }

    void thenSystemUpdateStateShouldNotBeAltered()
    {
        QnJsonRestResult result;
        NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateInformation", &result);

        update::Information receivedUpdateInfo = result.deserialized<update::Information>();
        ASSERT_EQ(-1, receivedUpdateInfo.lastInstallationRequestTime);
        ASSERT_TRUE(receivedUpdateInfo.participants.isEmpty());
    }

    void thenGlobalUpdateInformationShouldContainParticipants(const QList<QnUuid>& participants)
    {
        QnJsonRestResult result;
        NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateInformation", &result);

        update::Information receivedUpdateInfo = result.deserialized<update::Information>();
        ASSERT_EQ(participants.size(), receivedUpdateInfo.participants.size());
        for (const auto& participant: receivedUpdateInfo.participants)
            ASSERT_TRUE(participants.contains(participant));
    }

    void thenGlobalUpdateInformationShouldContainCorrectLastInstallationRequestTime()
    {
        QnJsonRestResult result;
        NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateInformation", &result);

        update::Information receivedUpdateInfo = result.deserialized<update::Information>();
        ASSERT_NE(-1, receivedUpdateInfo.lastInstallationRequestTime);
    }

    void whenServersRestartedWithNewVersions()
    {
        for (const auto& peer: m_peers)
            peer->stop();

        for (const auto& peer: m_peers)
        {
            peer->addCmdOption("--override-version=4.0.0.1");
            ASSERT_TRUE(peer->start());
        }

        connectPeers();
    }

    void thenFinishUpdateShouldSucceed()
    {
        NX_TEST_API_POST(m_peers[0].get(), "/ec2/finishUpdate", "");

        QnRestResult result;
        NX_TEST_API_GET(m_peers[0].get(), "/ec2/updateInformation", &result);
        ASSERT_NE(QnRestResult::NoError, result.error);
    }

    void whenServerRestartedWithNewVersion(int peerIndex, const std::string& version = "4.0.0.1")
    {
        m_peers[peerIndex]->stop();
        m_peers[peerIndex]->addCmdOption("--override-version=" + version);
        ASSERT_TRUE(m_peers[peerIndex]->start());
    }

    void whenServerRestartedWithOldVersion(int peerIndex)
    {
        m_peers[peerIndex]->stop();
        m_peers[peerIndex]->addCmdOption("--override-version=4.0.0.0");
        ASSERT_TRUE(m_peers[peerIndex]->start());
    }

    void whenServersConnected()
    {
        connectPeers();
    }

    void thenFinishUpdateShouldFail(QnRestResult::Error expectedCode)
    {
        issueFinishUpdateRequestAndAssertResponse(false, expectedCode);
    }

    void thenFinishUpdateWithIgnorePendingPeersShouldSucceed()
    {
        issueFinishUpdateRequestAndAssertResponse(true, QnRestResult::NoError);
    }

    void whenPeerGoesOffline(int peerIndex)
    {
        m_peers[peerIndex]->stop();
    }

    void assertForbiddenStatus(
        const std::vector<QString>& requestsToCheck,
        const UserDataWithExpectedForbiddenStatus& userDataWithHttpCode)
    {
        for (const auto& request: requestsToCheck)
        {
            NX_TEST_API_POST(
                peer(0), request, "{}", nullptr,
                userDataWithHttpCode.shouldBeForbidden
                    ? StatusCodeExpectation(Equals(nx::network::http::StatusCode::forbidden))
                    : StatusCodeExpectation(NotEquals(nx::network::http::StatusCode::forbidden)),
                userDataWithHttpCode.userData.name,
                userDataWithHttpCode.userData.password);
        }
    }

    MediaServerLauncher* peer(size_t index) { return m_peers[index].get(); }

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
    nx::network::http::TestHttpServer m_testHttpServer;

    QDir m_keysDir;

    void connectPeers()
    {
        for (int i = 0; i < m_peers.size() - 1; ++i)
        {
            const auto& to = m_peers[i + 1];
            const auto toUrl = nx::utils::url::parseUrlFields(to->endpoint().toString(),
                nx::network::http::kUrlSchemeName);

            m_peers[i]->serverModule()->ec2Connection()->messageBus()->addOutgoingConnectionToPeer(
                to->commonModule()->moduleGUID(), nx::vms::api::PeerType::server, toUrl);
        }

        const auto firstPeerMessageBus = m_peers[0]->serverModule()->ec2Connection()->messageBus();
        int distance = std::numeric_limits<int>::max();
        while (distance > m_peers.size())
        {
            distance = firstPeerMessageBus->distanceToPeer(
                m_peers[m_peers.size() - 1]->commonModule()->moduleGUID());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void issueAndCheckUpdateInformationRequest(
        bool shouldBeEmpty = false,
        const QString& versionKey = QString())
    {
        QString path = "/ec2/updateInformation";
        if (!versionKey.isNull())
            path += "?version=" + versionKey;

        if (shouldBeEmpty)
        {
            QnRestResult result;
            NX_TEST_API_GET(m_peers[0].get(), path, &result);
            ASSERT_NE(QnRestResult::NoError, result.error);
        }
        else
        {
            QnJsonRestResult result;
            do
            {
                NX_TEST_API_GET(m_peers[0].get(), path, &result);
            }
            while (m_updateInformation.version != result.deserialized<nx::update::Information>().version);
        }
    }

    void setUpdateInformation(const QList<QnUuid>& participants = QList<QnUuid>())
    {
        prepareCorrectUpdateInformation(participants);
        QByteArray serializedUpdateInfo;
        NX_TEST_API_POST(m_peers[0].get(), "/ec2/startUpdate", m_updateInformation);
    }

    void issueInstallUpdateRequest(
        bool hasPeers, const QList<QnUuid>& participants, QnRestResult::Error expectedRestResult)
    {
        QByteArray responseBuffer;
        QString path = "/api/installUpdate";

        if (hasPeers)
        {
            path += "?peers=";
            for (int i = 0; i < participants.size(); ++i)
            {
                path += participants[i].toString();
                if (i != participants.size() - 1)
                    path += ',';
            }
        }

        while (true)
        {
            NX_TEST_API_POST(
                m_peers[0].get(), path, "", nullptr,
                Equals(nx::network::http::StatusCode::ok), "admin", "admin", &responseBuffer);
            QnRestResult installResponse;
            QJson::deserialize(responseBuffer, &installResponse);
            if (expectedRestResult != installResponse.error)
            {
                NX_DEBUG(
                    this, "Waiting for  install request to return expected result: %1",
                    expectedRestResult);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            else
            {
                break;
            }
        }
    }

    void issueFinishUpdateRequestAndAssertResponse(bool ignorePendingPeers,
        QnRestResult::Error expectedCode)
    {
        QString path = "/ec2/finishUpdate";
        if (ignorePendingPeers)
            path += "?ignorePendingPeers";

        QByteArray responseBuffer;
        NX_TEST_API_POST(
            m_peers[0].get(), path, "", nullptr,
            Equals(nx::network::http::StatusCode::ok), "admin", "admin", &responseBuffer);

        QnRestResult installResponse;
        QJson::deserialize(responseBuffer, &installResponse);
        ASSERT_EQ(expectedCode, installResponse.error);
    }

    void prepareCorrectUpdateInformation(const QList<QnUuid>& participants = QList<QnUuid>())
    {
        m_updateInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        m_updateInformation.version = "4.0.0.1";
        m_updateInformation.packages.append(preparePackage(m_peers[0]->dataDir()));
        m_updateInformation.participants = participants;
    }

    update::Package preparePackage(const QString& dataDir)
    {
        update::Package result;

        const auto& osInfo = nx::utils::OsInfo::current();

        result.component = "server";

        QString updateFileFullPath;
        result.file = createUpdateZip(dataDir, &result.md5, &result.size, &updateFileFullPath);
        result.url = serveUpdateFile(updateFileFullPath);
        result.platform = osInfo.platform;
        result.variants.append({osInfo.variant, osInfo.variantVersion});

        auto signResult = common::FileSignature::sign(updateFileFullPath, kPrivateKey);
        NX_ASSERT(std::holds_alternative<QByteArray>(signResult));
        result.signature = std::get<QByteArray>(signResult);

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

        const auto& osInfo = nx::utils::OsInfo::current();

        update::PackageInformation packageInfo;
        packageInfo.version = "4.0.0.1";
        packageInfo.component = "server";
        packageInfo.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        packageInfo.platform = osInfo.platform;
        packageInfo.variants.append({osInfo.variant, osInfo.variantVersion});
        packageInfo.installScript = "install";
        const QByteArray& packageInfoJsonContent = QJson::serialized(packageInfo);

        const QString zipFileName = "update.zip";
        const QString zipFilePath = updateFilesPath + zipFileName;

        QList<ZipContext> zipContextList = {
            ZipContext("package.json", packageInfoJsonContent),
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
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
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

TEST_F(FtUpdates, updateInformation_onePeer_correctlySet)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, installUpdate_wontWorkWithoutPeersParameter)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithoutPeersParameterShouldFail();
}

TEST_F(FtUpdates, installUpdate_willWorkOnlyWithPeersParameter)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    QList<QnUuid> participants{peerId(0), peerId(1)};
    thenInstallUpdateWithPeersParameterShouldSucceed(participants);
    thenGlobalUpdateInformationShouldContainParticipants(participants);
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest(participants);
}

TEST_F(FtUpdates, installUpdate_timestampCorrectlySet)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldSucceed({peerId(0)});
    thenGlobalUpdateInformationShouldContainCorrectLastInstallationRequestTime();
}

TEST_F(FtUpdates, installUpdate_onlyParticipantsReceiveRequest)
{
    givenConnectedPeers(3);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall},
        {peerId(2), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    QList<QnUuid> participants{peerId(0), peerId(1)};
    thenInstallUpdateWithPeersParameterShouldSucceed(participants);
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest(participants);
    thenGlobalUpdateInformationShouldContainParticipants(participants);
}

TEST_F(FtUpdates, installUpdate_participantWithNewerVersionDoesNotReceiveRequest_2peers)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServerRestartedWithNewVersion(1, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldFail({peerId(0), peerId(1)});
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({peerId(0)});
}

TEST_F(FtUpdates, installUpdate_participantViaNewerPeerReceiveRequest)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{peerId(0), peerId(1)});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(1), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldSucceed({ peerId(0), peerId(1) });
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({ peerId(0), peerId(1) });
}

TEST_F(FtUpdates, installUpdate_failIfParticipantIsOffline)
{
    givenConnectedPeers(3);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall},
        {peerId(2), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    const auto peer2Id = peerId(2);
    whenPeerGoesOffline(2);

    QList<QnUuid> participants{ peerId(0), peerId(1), peer2Id };
    thenInstallUpdateWithPeersParameterShouldFail(participants);
    thenSystemUpdateStateShouldNotBeAltered();
}

TEST_F(FtUpdates, installUpdate_fail_emptyUpdateInformation)
{
    givenConnectedPeers(2);

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::idle},
        {peerId(1), update::Status::Code::idle},
    };
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldFail({ peerId(0), peerId(1) });
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({ peerId(0) });
}

TEST_F(FtUpdates, updateStatus_nonParticipantsAreNotInStatusesList)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{peerId(0)});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_allParticipantsAreInStatusesList)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{peerId(0), peerId(1)});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_allParticipantsAreInStatusesList_requestToNonParticipant)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{peerId(1)});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_nonParticipantInList_requestisLocal)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{peerId(1)});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::idle} };
    thenPeersUpdateStatusShouldBe(expectedStatuses, /*isRequestLocal*/ true);
}

TEST_F(FtUpdates, updateStatus_allParticipantsAreInStatusesList_partcipantsListIsEmpty)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_participantHasNewerVersion_partcipantsListIsEmpty)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    whenServerRestartedWithNewVersion(1, "4.0.0.2");
    whenServersConnected();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_partcipantsListIsEmpty_serversHaveNewerVersion)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServerRestartedWithNewVersion(1, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, updateStatus_partcipantsListIsEmpty_serversHaveNewerVersion_isLocal)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServerRestartedWithNewVersion(1, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::error}};
    thenPeersUpdateStatusShouldBe(expectedStatuses, /*isLocal*/ true);
}

TEST_F(FtUpdates, updateStatus_emptyUpdateInformation_allShouldBeInList)
{
    givenConnectedPeers(2);
    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::idle},
        {peerId(1), update::Status::Code::idle},
    };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
}

TEST_F(FtUpdates, finishUpdate_success_updateInformationCleared)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);
    thenInstallUpdateWithPeersParameterShouldSucceed({});

    whenServersRestartedWithNewVersions();
    thenFinishUpdateShouldSucceed();
}

TEST_F(FtUpdates, finishUpdate_fail_notAllUpdated)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
    thenInstallUpdateWithPeersParameterShouldSucceed({});

    whenServerRestartedWithNewVersion(0);
    whenServerRestartedWithOldVersion(1);
    whenServersConnected();
    thenFinishUpdateShouldFail(QnRestResult::CantProcessRequest);
}

TEST_F(FtUpdates, finishUpdate_fail_participantWithOldVersionIsOffline)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
    thenInstallUpdateWithPeersParameterShouldSucceed({peerId(0), peerId(1)});

    whenPeerGoesOffline(1);

    whenServerRestartedWithNewVersion(0);
    thenFinishUpdateShouldFail(QnRestResult::CantProcessRequest);
}

TEST_F(FtUpdates, finishUpdate_success_participantWithNewVersionIsOffline)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
    thenInstallUpdateWithPeersParameterShouldSucceed({ peerId(0), peerId(1) });

    whenServerRestartedWithNewVersion(1);
    whenPeerGoesOffline(1);

    whenServerRestartedWithNewVersion(0);
    thenFinishUpdateShouldFail(QnRestResult::CantProcessRequest);
}

TEST_F(FtUpdates, finishUpdate_success_notAllUpdated_ignorePendingPeers)
{
    givenConnectedPeers(2);
    whenCorrectUpdateInformationWithEmptyParticipantListSet();
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall} };
    thenPeersUpdateStatusShouldBe(expectedStatuses);
    thenInstallUpdateWithPeersParameterShouldSucceed({});

    whenServerRestartedWithNewVersion(0);
    whenServerRestartedWithOldVersion(1);
    whenServersConnected();
    thenFinishUpdateWithIgnorePendingPeersShouldSucceed();
}

TEST_F(FtUpdates, finishUpdate_success_participantsHaveVersionNewerThanTarget)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServerRestartedWithNewVersion(1, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldFail({ peerId(0), peerId(1) });
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({ peerId(0) });
    thenFinishUpdateWithIgnorePendingPeersShouldSucceed();
}

TEST_F(FtUpdates, finishUpdate_success_oneParticipantHasVersionNewerThanTarget)
{
    givenConnectedPeers(2);
    whenServerRestartedWithNewVersion(0, "4.0.0.2");
    whenServersConnected();

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(1), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldSucceed({ peerId(0), peerId(1) });
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({ peerId(0), peerId(1) });
    thenFinishUpdateWithIgnorePendingPeersShouldSucceed();
}

TEST_F(FtUpdates, finishUpdate_success_installedUpdateInformationSetCorrectly)
{
    givenConnectedPeers(2);

    whenCorrectUpdateInformationWithParticipantsSet(QList<QnUuid>{});
    thenTargetUpdateInformationShouldBeRetrievable();
    thenTargetUpdateInformationShouldBeRetrievable(/*shouldBeEmpty*/ false, "target");
    thenInstalledtUpdateInformationShouldBeRetrievable(/*shouldBeEmpty*/ true);

    QMap<QnUuid, update::Status::Code> expectedStatuses{
        {peerId(0), update::Status::Code::readyToInstall},
        {peerId(1), update::Status::Code::readyToInstall}};
    thenPeersUpdateStatusShouldBe(expectedStatuses);

    thenInstallUpdateWithPeersParameterShouldSucceed({ peerId(0), peerId(1) });
    thenOnlyParticipantsShouldHaveReceivedInstallUpdateRequest({ peerId(0), peerId(1) });

    whenServersRestartedWithNewVersions();

    thenFinishUpdateShouldSucceed();
    thenInstalledtUpdateInformationShouldBeRetrievable(/*shouldBeEmpty*/ false);
    thenTargetUpdateInformationShouldBeRetrievable(/*shouldBeEmpty*/ true, QString());
    thenTargetUpdateInformationShouldBeRetrievable(/*shouldBeEmpty*/ true, "target");
}

TEST_F(FtUpdates, UpdateRequestsAreRestrictedToAdminOnly)
{
    givenConnectedPeers(1);
    const std::vector<QString> requestsToCheck = {
        "/ec2/startUpdate",
        "/ec2/cancelUpdate",
        "/ec2/finishUpdate",
        "/ec2/retryUpdate",
        "/api/installUpdate?peers="
    };

    const std::vector<UserDataWithExpectedForbiddenStatus> userDataWithExpectedResults = {
        { test_support::createUser(peer(0), GlobalPermission::advancedViewerPermissions, "viewer"), forbidden(true) },
        { test_support::createUser(peer(0), GlobalPermission::accessAllMedia, "access"), forbidden(true) },
        { test_support::createUser(peer(0), GlobalPermission::admin, "administrator"), forbidden(false) },
        { test_support::getOwner(peer(0)), forbidden(false) }
    };

    for (const auto& userDataToCheck: userDataWithExpectedResults)
        assertForbiddenStatus(requestsToCheck, userDataToCheck);
}

} // namespace nx::vms::server::test
