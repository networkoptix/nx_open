////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "launching_application.h"

#include <QProcess>

#include <utils/common/log.h>

#include "../installation_manager.h"
#include "../launcher_common_data.h"


#ifdef _WIN32
static const QLatin1String APPLICATION_BIN_NAME( "client.exe" );
#else
static const QLatin1String APPLICATION_BIN_NAME( "client" );
#endif

LaunchingApplication::LaunchingApplication(
    QState* const parent,
    const LauncherCommonData& launcherCommonData,
    const InstallationManager& installationManager )
:
    QState( parent ),
    m_launcherCommonData( launcherCommonData ),
    m_installationManager( installationManager )
{
}

void LaunchingApplication::onEntry( QEvent* _event )
{
    QState::onEntry( _event );

    NX_LOG( QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1 );

    InstallationManager::AppData appData;
    if( !m_installationManager.getInstalledVersionData( ApplicationVersionData::fromString(m_launcherCommonData.currentTask.version), &appData ) )
    {
        emit failed();
        return;
    }

    if( QProcess::startDetached(
            appData.installationDirectory + "/" + APPLICATION_BIN_NAME,
            m_launcherCommonData.currentTask.appArgs.split(QLatin1String(" "), QString::SkipEmptyParts) ) )
        emit succeeded();
    else
        emit failed();
}
