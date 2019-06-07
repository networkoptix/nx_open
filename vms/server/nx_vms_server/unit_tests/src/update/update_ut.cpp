#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <transaction/message_bus_adapter.h>
#include <api/test_api_requests.h>
#include <api/global_settings.h>
#include <nx/update/update_information.h>
#include <nx/network/http/test_http_server.h>
#include <quazip/quazipfile.h>
#include <rest/server/json_rest_handler.h>
#include <common/common_module.h>

namespace nx::vms::server::test {

static const QString kUpdateFilesPathPostfix = "/test_update_files/";

using namespace nx::test;

class FtUpdates: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_testHttpServer.bindAndListen());
    }

    void givenConnectedPeers(int count)
    {

        const QnUuid systemId = QnUuid::createUuid();
        for (int i = 0; i < count; ++i)
        {
            m_peers.emplace_back(std::make_unique<MediaServerLauncher>(QString()));

            m_peers.back()->addCmdOption("--override-version=4.0.0.0");
            m_peers.back()->addSetting("ignoreRootTool", "true");
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

    void issueInstallUpdateRequest(bool hasPeers, const QList<QnUuid>& participants,
        QnRestResult::Error expectedRestResult)
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
            NX_TEST_API_POST(m_peers[0].get(), path, "", nullptr,
                nx::network::http::StatusCode::ok, "admin", "admin", &responseBuffer);
            QnRestResult installResponse;
            QJson::deserialize(responseBuffer, &installResponse);
            if (expectedRestResult != installResponse.error)
            {
                NX_DEBUG(
                    this, "Waiting for  install request to return expected result:",
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
        NX_TEST_API_POST(m_peers[0].get(), path, "", nullptr,
            nx::network::http::StatusCode::ok, "admin", "admin", &responseBuffer);

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
    thenGlobalUpdateInformationShouldContainParticipants(participants);
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

} // namespace nx::vms::server::test
