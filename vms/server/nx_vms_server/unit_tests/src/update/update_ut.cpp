#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include <test_support/mediaserver_launcher.h>
#include <transaction/message_bus_adapter.h>
#include <api/test_api_requests.h>
#include <nx/update/update_information.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/network/http/test_http_server.h>

namespace nx::vms::server::test {

class Updates: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
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
            const auto toUrl = utils::url::parseUrlFields(
                to->endpoint().toString(),
                network::http::kUrlSchemeName);
            m_peers[i]->serverModule()->ec2Connection()->messageBus()->addOutgoingConnectionToPeer(
                to->commonModule()->moduleGUID(),
                vms::api::PeerType::server,
                toUrl);
        }
    }

    void whenCorrectUpdateInformationSet()
    {
        m_updateInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        m_updateInformation.version = "4.0.0.1";
        update::Package winPackage;
        winPackage.arch = QnAppInfo::applicationArch();
        winPackage.component = "server";
        winPackage.file = "test_file.zip";
        winPackage.md5 ="";
        winPackage.platform = "windows";
        winPackage.size = 1;
        winPackage.url = "url";
        winPackage.variant = "winxp";
        winPackage.variantVersion = "10.0";
        //m_updateInformation.packages.
    }

    void thenItShouldBeRetrivable()
    {
    }

private:
    std::vector<std::unique_ptr<MediaServerLauncher>> m_peers;
    nx::update::Information m_updateInformation;
    network::http::TestHttpServer m_testHttpServer;

    update::Package preparePackage(const QString& dataDir)
    {
        update::Package result;
        result.arch = QnAppInfo::applicationArch();
        result.component = "server";
        result.file = result.arch + "_update.zip";

        QDir().mkdir(dataDir + '/tmp');

    }
};

TEST_F(Updates, updateInformation_onePeer_correctlySet)
{
    givenConnectedPeers(1);
    whenCorrectUpdateInformationSet();
    thenItShouldBeRetrivable();
}

} // namespace nx::vms::server::test
