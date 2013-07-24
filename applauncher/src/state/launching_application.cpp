////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "launching_application.h"

#include <QDebug>
#include <QProcess>

#include <utils/common/log.h>

#include "version.h"
#include "../installation_manager.h"
#include "../launcher_common_data.h"


LaunchingApplication::LaunchingApplication(
    QState* const parent,
    const LauncherCommonData& launcherCommonData,
    const InstallationManager& installationManager,
    QSettings* const settings )
:
    QState( parent ),
    m_launcherCommonData( launcherCommonData ),
    m_installationManager( installationManager ),
    m_settings( settings )
{
}

void LaunchingApplication::prepareResultMessage()
{
    //TODO/IMPL
}

#ifdef AK_DEBUG
static const QString APPLICATION_BIN_NAME( QString::fromLatin1("/%1").arg(QLatin1String("client.exe")) );
#else
static const QString APPLICATION_BIN_NAME( QString::fromLatin1("/%1").arg(QLatin1String(QN_CLIENT_EXECUTABLE_NAME)) );
#endif

static const QLatin1String NON_RECENT_VERSION_ARGS_PARAM_NAME( "nonRecentVersionArgs" );
static const QLatin1String NON_RECENT_VERSION_ARGS_DEFAULT_VALUE( "--updates-enabled=false" );

void LaunchingApplication::onEntry( QEvent* _event )
{
    QState::onEntry( _event );

    NX_LOG( QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1 );

    Q_ASSERT( m_launcherCommonData.currentTask->type == applauncher::api::TaskType::startApplication );
    applauncher::api::StartApplicationTask* startTask = m_launcherCommonData.currentTask.staticCast<applauncher::api::StartApplicationTask>().data();

    InstallationManager::AppData appData;
    for( ;; )
    {
        if( m_installationManager.getInstalledVersionData( startTask->version, &appData ) )
            break;

        if( m_installationManager.count() > 0 )
        {
            const QString& theMostRecentVersion = m_installationManager.getMostRecentVersion();
            if( startTask->version != theMostRecentVersion )
            {
                startTask->version = theMostRecentVersion;
                continue;
            }
        }
        NX_LOG( QString::fromLatin1("Failed to find installed version %1 path").arg(startTask->version), cl_logDEBUG1 );
        emit failed();
        return;
    }

    if( startTask->version != m_installationManager.getMostRecentVersion() )
        startTask->appArgs += QString::fromLatin1(" ") + m_settings->value( NON_RECENT_VERSION_ARGS_PARAM_NAME, NON_RECENT_VERSION_ARGS_DEFAULT_VALUE ).toString();

    const QString binPath = appData.installationDirectory + "/" + APPLICATION_BIN_NAME;
    NX_LOG( QString::fromLatin1("Launching version %1 (path %2)").arg(startTask->version).arg(binPath), cl_logDEBUG2 );
    if( QProcess::startDetached(
            binPath,
            startTask->appArgs.split(QLatin1String(" "), QString::SkipEmptyParts) ) )
    {
        NX_LOG( QString::fromLatin1("Successfully launched version %1 (path %2)").arg(startTask->version).arg(binPath), cl_logDEBUG1 );
        emit succeeded();
        m_settings->setValue( QLatin1String("previousLaunchedVersion"), startTask->version );
    }
    else
    {
        NX_LOG( QString::fromLatin1("Failed to launch version %1 (path %2)").arg(startTask->version).arg(binPath), cl_logDEBUG1 );
        emit failed();
    }
}
