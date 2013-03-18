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
    return m_installationManager.isVersionInstalled( ApplicationVersionData::fromString(currentTask.version) );
}

bool LauncherCommonData::areThereAnyVersionInstalledBesidesJustTriedOne() const
{
    //TODO/IMPL
    return false;
}
