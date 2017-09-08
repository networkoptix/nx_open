#include "relay_service.h"

#include "controller/controller.h"
#include "model/model.h"
#include "view/view.h"
#include "settings.h"
#include "libtraffic_relay_app_info.h"

namespace nx {
namespace cloud {
namespace relay {

RelayService::RelayService(int argc, char **argv):
    base_type(argc, argv, AppInfo::applicationDisplayName())
{
}

std::vector<SocketAddress> RelayService::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

const model::ListeningPeerPool& RelayService::listeningPeerPool() const
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
    m_model = &model;

    Controller controller(settings, &model);
    if (!controller.discoverPublicAddress())
    {
        NX_ERROR(this, "Failed to discover public address. Terminating.");
        return -1;
    }

    View view(settings, model, &controller);
    m_view = &view;

    // TODO: #ak: process rights reduction should be done here.

    view.start();

    return runMainLoop();
}

} // namespace relay
} // namespace cloud
} // namespace nx
