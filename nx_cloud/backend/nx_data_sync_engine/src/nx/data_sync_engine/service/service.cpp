#include "service.h"

#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/type_utils.h>

#include "controller.h"
#include "model.h"
#include "settings.h"
#include "view.h"

namespace nx::data_sync_engine {

Service::Service(
    const std::string& applicationId,
    int argc,
    char **argv)
    :
    base_type(argc, argv, "xxx"),
    m_applicationId(applicationId)
{
}

std::vector<network::SocketAddress> Service::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

void Service::connectToNode(const nx::utils::Url& /*url*/)
{
    // TODO
}

std::unique_ptr<utils::AbstractServiceSettings> Service::createSettings()
{
    return std::make_unique<Settings>();
}

int Service::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const Settings& settings = static_cast<const Settings&>(abstractSettings);

    m_settings = &settings;

    Model model(settings);
    m_model = &model;

    Controller controller(m_applicationId, settings, &model);
    m_controller = &controller;

    View view(settings, &controller);
    m_view = &view;

    setup();
    auto customLogicGuard = nx::utils::makeScopeGuard([this]() { teardown(); });

    // Process privilege reduction.
    //nx::utils::CurrentProcess::changeUser(settings.changeUser());

    view.listen();
    NX_LOGX(lm("Listening on %1").container(httpEndpoints()), cl_logINFO);

    NX_LOG(lm("%1 has been started").arg(applicationDisplayName()), cl_logALWAYS);

    const auto result = runMainLoop();

    NX_ALWAYS(this, lm("Stopping..."));

    return result;
}

nx::sql::AsyncSqlQueryExecutor& Service::sqlQueryExecutor()
{
    return m_model->queryExecutor();
}

SyncronizationEngine& Service::syncronizationEngine()
{
    return m_controller->syncronizationEngine();
}

} // namespace nx::data_sync_engine
