////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "installing_application.h"

#include <QDir>
#include <QIODevice>

#include <utils/common/log.h>

#include "../installation_manager.h"
#include "../launcher_common_data.h"


InstallingApplication::InstallingApplication(
    QState* const parent,
    const LauncherCommonData& launcherCommonData,
    InstallationManager* const installationManager )
:
    QState( parent ),
    m_launcherCommonData( launcherCommonData ),
    m_installationManager( installationManager )
{
}

void InstallingApplication::prepareResultMessage()
{
    //TODO/IMPL
}

void InstallingApplication::onEntry( QEvent* _event )
{
    QState::onEntry( _event );

    NX_LOG( QString::fromLatin1("Entered InstallingApplication"), cl_logDEBUG1 );

    if( m_installationManager->install( m_launcherCommonData.downloadedFilePath ) )
        emit succeeded();
    else
        emit failed();
}
