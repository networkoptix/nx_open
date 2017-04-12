#include "relay_process.h"

#include "controller/controller.h"
#include "model/model.h"
#include "view/view.h"
#include "settings.h"
#include "libtraffic_relay_app_info.h"

namespace nx {
namespace cloud {
namespace relay {

RelayProcess::RelayProcess(int argc, char **argv):
    base_type(argc, argv, TrafficRelayAppInfo::applicationDisplayName())
{
}

std::unique_ptr<utils::AbstractServiceSettings> RelayProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int RelayProcess::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    Model model(settings);
    Controller controller(settings, &model);
    View view(settings, model, &controller);

    // TODO: #ak: process rights reduction should be done here.

    view.start();

    return runMainLoop();
}

} // namespace relay
} // namespace cloud
} // namespace nx
