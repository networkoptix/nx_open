#include "relay_service.h"

#include "controller/controller.h"
#include "libtraffic_relay_app_info.h"
#include "model/model.h"
#include "settings.h"
#include "statistics_provider.h"
#include "view/view.h"

namespace nx {
namespace cloud {
namespace relay {

RelayService::RelayService(int argc, char **argv):
    base_type(argc, argv, AppInfo::applicationDisplayName())
{
}

std::vector<network::SocketAddress> RelayService::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

std::vector<network::SocketAddress> RelayService::httpsEndpoints() const
{
    return m_view->httpsEndpoints();
}

const relaying::AbstractListeningPeerPool& RelayService::listeningPeerPool() const
{
    return m_model->listeningPeerPool();
}

std::unique_ptr<utils::AbstractServiceSettings> RelayService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int RelayService::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    Model model(settings);
    while (!model.doMandatoryInitialization())
    {
        if (isTerminated())
            return -1;

        NX_INFO(this, lm("Retrying model initialization after delay"));
        std::this_thread::sleep_for(
            settings.cassandraConnection().delayBeforeRetryingInitialConnect);
    }
    m_model = &model;

    Controller controller(settings, &model);
    m_controller = &controller;

    View view(settings, &model, &controller);
    m_view = &view;

    auto statisticsProvider = StatisticsProviderFactory::instance().create(
        model.listeningPeerPool(),
        view.httpServer(),
        controller.trafficRelay());
    view.registerStatisticsApiHandlers(*statisticsProvider);

    if (!registerThisInstanceNameInCluster(settings))
        return -1;

    // TODO: #ak: process rights reduction should be done here.

    view.start();

    return runMainLoop();
}

bool RelayService::registerThisInstanceNameInCluster(const conf::Settings& settings)
{
    std::string externalHostName = settings.server().name;
    if (externalHostName.empty())
    {
        const auto publicIp = m_controller->discoverPublicAddress();
        if (!publicIp)
        {
            NX_ERROR(this, "Failed to discover public address. Terminating.");
            return false;
        }

        int port = 0;
        if (!m_view->httpEndpoints().empty())
            port = m_view->httpEndpoints().front().port;
        else
            port = m_view->httpsEndpoints().front().port;

        externalHostName = network::SocketAddress(*publicIp, port).toStdString();
    }

    m_model->remoteRelayPeerPool().setNodeId(externalHostName);

    return true;
}

} // namespace relay
} // namespace cloud
} // namespace nx
