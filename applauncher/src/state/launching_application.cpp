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

static const QString APPLICATION_BIN_NAME( QString::fromLatin1("/%1").arg(QLatin1String(QN_CLIENT_EXECUTABLE_NAME)) );

void LaunchingApplication::onEntry( QEvent* _event )
{
    QState::onEntry( _event );

    NX_LOG( QString::fromLatin1("Entered LaunchingApplication"), cl_logDEBUG1 );

    InstallationManager::AppData appData;
    if( !m_installationManager.getInstalledVersionData( m_launcherCommonData.currentTask.version, &appData ) )
    {
        NX_LOG( QString::fromLatin1("Failed to find installed version %1 path").arg(m_launcherCommonData.currentTask.version), cl_logDEBUG1 );
        emit failed();
        return;
    }

    const QString binPath = appData.installationDirectory + "/" + APPLICATION_BIN_NAME;
    NX_LOG( QString::fromLatin1("Launching version %1 (path %2)").arg(m_launcherCommonData.currentTask.version).arg(binPath), cl_logDEBUG2 );
    if( QProcess::startDetached(
            binPath,
            m_launcherCommonData.currentTask.appArgs.split(QLatin1String(" "), QString::SkipEmptyParts) ) )
    {
        NX_LOG( QString::fromLatin1("Successfully launched version %1 (path %2)").arg(m_launcherCommonData.currentTask.version).arg(binPath), cl_logDEBUG1 );
        emit succeeded();
        m_settings->setValue( QLatin1String("previousLaunchedVersion"), m_launcherCommonData.currentTask.version );
    }
    else
    {
        NX_LOG( QString::fromLatin1("Failed to launch version %1 (path %2)").arg(m_launcherCommonData.currentTask.version).arg(binPath), cl_logDEBUG1 );
        emit failed();
    }
}
