////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "launcher_common_data.h"

#include "installation_manager.h"


LauncherCommonData::LauncherCommonData( const InstallationManager& installationManager )
:
    m_installationManager( installationManager )
{
}

bool LauncherCommonData::isRequiredVersionInstalled() const
{
    Q_ASSERT( currentTask->type == applauncher::api::TaskType::startApplication );
    return m_installationManager.isVersionInstalled( currentTask.staticCast<applauncher::api::StartApplicationTask>()->version );
}

bool LauncherCommonData::areThereAnyVersionInstalledBesidesJustTriedOne() const
{
    //TODO/IMPL
    return false;
}

int LauncherCommonData::currentTaskType() const
{
    return currentTask->type;
}
