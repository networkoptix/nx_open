#include "self_updater.h"

#include <client/client_module.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/utils/app_info.h>
#include <nx/utils/platform/protocol_handler.h>

#include <utils/common/app_info.h>


nx::vms::client::SelfUpdater::SelfUpdater(const QnStartupParameters& startupParams) :
    m_clientVersion(QnAppInfo::applicationVersion())
{
    if (!startupParams.engineVersion.isEmpty())
        m_clientVersion = nx::utils::SoftwareVersion(startupParams.engineVersion);

    bool installationOk = registerUriHandler() && updateClientInstallation();

    /* If something must be updated AND client is not run under administrator, do it. */
    if (!installationOk && !startupParams.hasAdminPermissions)
    {
        /* Start another client instance with admin permissions if required. */
        nx::utils::runAsAdministratorWithUAC(qApp->applicationFilePath(),
                                             QStringList()
                                             << QnStartupParameters::kHasAdminPermissionsKey
                                             << QnStartupParameters::kAllowMultipleClientInstancesKey);
    }

}

bool nx::vms::client::SelfUpdater::registerUriHandler()
{
    return nx::utils::registerSystemUriProtocolHandler(nx::vms::utils::AppInfo::nativeUriProtocol(),
                                                       qApp->applicationFilePath(),
                                                       nx::vms::utils::AppInfo::nativeUriProtocolDescription(),
                                                       m_clientVersion);
}

bool nx::vms::client::SelfUpdater::updateClientInstallation()
{
    return true;

    /* Following code will be required a bit later. */

    QnClientModule::initApplication();
    QString appPath = QDir(qApp->applicationFilePath()).canonicalPath();
    QDir dataLocation(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + lit("/../client/"));
    dataLocation.makeAbsolute();

    /* Check that we are running downloaded version. */
    bool runningDownloadedApp = appPath.startsWith(dataLocation.canonicalPath());

    /* Update process must be run only if we are running client from userData folder. */
    if (!runningDownloadedApp)
        return true;

    /* Check that installed version is older. */
    const QString installationPath = QnAppInfo::installationRoot();

    return true;
}
