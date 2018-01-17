#include "cloud_db_service.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>
#include <type_traits>

#include <QtCore/QDir>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>
#include <nx/utils/db/db_structure_updater.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/type_utils.h>

#include "controller.h"
#include "http_view.h"
#include "libcloud_db_app_info.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(libcloud_db);
    return 0;
}

namespace nx {
namespace cdb {

CloudDbService::CloudDbService(int argc, char **argv):
    base_type(argc, argv, QnLibCloudDbAppInfo::applicationDisplayName()),
    m_settings(nullptr)
{
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

std::vector<network::SocketAddress> CloudDbService::httpEndpoints() const
{
    return m_view->endpoints();
}

std::unique_ptr<utils::AbstractServiceSettings> CloudDbService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int CloudDbService::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    auto logSettings = settings.vmsSynchronizationLogging();
    logSettings.updateDirectoryIfEmpty(settings.dataDir());
    nx::utils::log::initialize(
        logSettings, QnLibCloudDbAppInfo::applicationDisplayName(), QString(),
        "sync_log", nx::utils::log::addLogger({QnLog::EC2_TRAN_LOG}));

    m_settings = &settings;

    Controller controller(settings);

    HttpView view(settings, &controller);
    m_view = &view;
    view.bind();

    // Process privilege reduction.
    nx::utils::CurrentProcess::changeUser(settings.changeUser());

    view.listen();
    NX_LOGX(lm("Listening on %1").container(view.endpoints()), cl_logINFO);

    NX_LOG(lm("%1 has been started")
        .arg(QnLibCloudDbAppInfo::applicationDisplayName()), cl_logALWAYS);

    const auto result = runMainLoop();

    NX_ALWAYS(this, lm("Stopping..."));

    return result;
}

} // namespace cdb
} // namespace nx
