#include "cloud_db_service.h"

#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/type_utils.h>

#include "controller.h"
#include "model.h"
#include "http_view.h"
#include "libcloud_db_app_info.h"
#include "statistics/provider.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(libcloud_db);
    return 0;
}

namespace nx::cloud::db {

CloudDbService::CloudDbService(int argc, char **argv):
    base_type(argc, argv, QnLibCloudDbAppInfo::applicationDisplayName()),
    m_settings(nullptr)
{
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cloud::db::libcloud_db and fail...
    registerQtResources();
}

std::vector<network::SocketAddress> CloudDbService::httpEndpoints() const
{
    return m_view->endpoints();
}

Controller& CloudDbService::controller()
{
    return *m_controller;
}

std::unique_ptr<utils::AbstractServiceSettings> CloudDbService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int CloudDbService::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    auto logSettings = settings.vmsSynchronizationLogging();
    for (auto& loggerSettings: logSettings.loggers)
        loggerSettings.logBaseName = "sync_log";
    logSettings.updateDirectoryIfEmpty(settings.dataDir());
    nx::utils::log::addLogger(
        nx::utils::log::buildLogger(
            logSettings,
            QnLibCloudDbAppInfo::applicationDisplayName(),
            QString(),
            {nx::utils::log::Filter(QnLog::EC2_TRAN_LOG)}));

    m_settings = &settings;

    auto model = ModelFactory::instance().create();

    Controller controller(settings, &model);
    m_controller = &controller;

    HttpView view(settings, &controller);
    m_view = &view;
    view.bind();

    statistics::Provider statisticsProvider(
        view.httpServer(),
        controller.ec2SyncronizationEngine().statisticsProvider());
    view.registerStatisticsApiHandlers(&statisticsProvider);

    // Process privilege reduction.
    nx::utils::CurrentProcess::changeUser(settings.changeUser());

    view.listen();
    NX_INFO(this, lm("Listening on %1").container(view.endpoints()));

    NX_ALWAYS(this, lm("%1 has been started")
        .arg(QnLibCloudDbAppInfo::applicationDisplayName()));

    const auto result = runMainLoop();

    NX_ALWAYS(this, lm("Stopping..."));

    return result;
}

} // namespace nx::cloud::db
