////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LAUNCHER_COMMON_DATA_H
#define LAUNCHER_COMMON_DATA_H

#include <QObject>
#include <QSharedPointer>

#include <api/start_application_task.h>


class InstallationManager;

//!Used to pass arguments between states
class LauncherCommonData
:
    public QObject
{
    Q_OBJECT

    //!true, if current task version is installed
    Q_PROPERTY( bool isRequiredVersionInstalled READ isRequiredVersionInstalled )
    //!true, if there is installed version different from current task version
    Q_PROPERTY( bool areThereAnyVersionInstalledBesidesJustTriedOne READ areThereAnyVersionInstalledBesidesJustTriedOne )
    Q_PROPERTY( int currentTaskType READ currentTaskType )

public: 
    QSharedPointer<applauncher::api::BaseTask> currentTask;
    QString downloadedFilePath;

    LauncherCommonData( const InstallationManager& installationManager );

    bool isRequiredVersionInstalled() const;
    bool areThereAnyVersionInstalledBesidesJustTriedOne() const;
    int currentTaskType() const;

private:
    const InstallationManager& m_installationManager;
    applauncher::api::StartApplicationTask m_currentTask;
};

#endif  //LAUNCHER_COMMON_DATA_H
