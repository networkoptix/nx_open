#include "service.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/type_utils.h>

#include "controller.h"
#include "model.h"
#include "settings.h"
#include "view.h"
#include "../http/http_paths.h"

namespace nx::clusterdb::engine {

Service::Service(
    const std::string& applicationId,
    int argc,
    char **argv)
    :
    base_type(argc, argv, "nx::clusterdb::engine service"),
    m_applicationId(applicationId),
    m_protocolVersionRange(ProtocolVersionRange::any)
{
}

void Service::setSupportedProtocolRange(
    const ProtocolVersionRange& protocolVersionRange)
{
    m_protocolVersionRange = protocolVersionRange;
}

ProtocolVersionRange Service::protocolVersionRange() const
{
    return m_protocolVersionRange;
}

std::vector<network::SocketAddress> Service::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

nx::utils::Url Service::synchronizationUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_view->httpEndpoints().front())
        .setPath(m_outgoingConnectionBasePath);
}

std::string Service::clusterId() const
{
    return m_settings->synchronization().clusterId;
}

void Service::connectToNode(const nx::utils::Url& url)
{
    using namespace nx::network;

    const auto syncApiTargetUrl =
        url::Builder(url).appendPath(m_outgoingConnectionBasePath).toUrl();

    m_controller->synchronizationEngine().connector().addNodeUrl(
        m_settings->synchronization().clusterId,
        syncApiTargetUrl);
}

void Service::disconnectFromNode(const nx::utils::Url& url)
{
    using namespace nx::network;

    const auto syncApiTargetUrl =
        url::Builder(url).appendPath(m_outgoingConnectionBasePath).toUrl();

    m_controller->synchronizationEngine().connector().removeNodeUrl(
        m_settings->synchronization().clusterId,
        syncApiTargetUrl);
}

nx::sql::AsyncSqlQueryExecutor& Service::sqlQueryExecutor()
{
    return m_model->queryExecutor();
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

    Controller controller(
        m_applicationId,
        settings,
        m_protocolVersionRange,
        &model);
    m_controller = &controller;

    View view(settings, settings.api().baseHttpPath, &controller);
    m_view = &view;

    // TODO: #ak Replace kClusterIdParamName unconditionally.
    m_outgoingConnectionBasePath = settings.api().baseHttpPath;
    if (m_outgoingConnectionBasePath.find(kClusterIdParamName) != std::string::npos)
    {
        m_outgoingConnectionBasePath = nx::network::http::rest::substituteParameters(
            m_outgoingConnectionBasePath, {settings.synchronization().clusterId});
    }

    setup(abstractSettings);
    auto customLogicGuard = nx::utils::makeScopeGuard([this]() { teardown(); });

    if (settings.discovery().enabled)
    {
        controller.synchronizationEngine().discoveryManager().start(
            settings.synchronization().clusterId,
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(view.httpEndpoints().front())
                .setPath(m_outgoingConnectionBasePath).toUrl());
    }

    // Process privilege reduction.
    //nx::utils::CurrentProcess::changeUser(settings.changeUser());

    view.listen();
    NX_INFO(this, lm("Listening on %1").container(httpEndpoints()));

    NX_ALWAYS(this, lm("%1 has been started").arg(applicationDisplayName()));

    const auto result = runMainLoop();

    NX_ALWAYS(this, lm("Stopping..."));

    return result;
}

SynchronizationEngine& Service::synchronizationEngine()
{
    return m_controller->synchronizationEngine();
}

const SynchronizationEngine& Service::synchronizationEngine() const
{
    return m_controller->synchronizationEngine();
}

} // namespace nx::clusterdb::engine
