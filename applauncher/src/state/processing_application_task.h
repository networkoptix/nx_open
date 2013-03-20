////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef PROCESSING_APPLICATION_TASK_H
#define PROCESSING_APPLICATION_TASK_H

#include <QSettings>
#include <QSharedPointer>
#include <QState>

#include <api/start_application_task.h>

#include "installation_manager.h"
#include "../blocking_queue.h"


class InstallationManager;
class LauncherCommonData;

class ProcessingApplicationTask
:
    public QState
{
    Q_OBJECT

public:
    ProcessingApplicationTask(
        QState* const parent,
        QSettings* const settings,
        InstallationManager* const installationManager,
        LauncherCommonData* const fsmSharedData,
        BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const taskQueue );

protected:
    virtual void onEntry( QEvent* _event ) override;
    virtual void onExit( QEvent* _event ) override;

private:
    QSettings* const m_settings;
    InstallationManager* const m_installationManager;
    LauncherCommonData* const m_fsmSharedData;
    BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const m_taskQueue;

    void initFsm();
};

#endif  //PROCESSING_APPLICATION_TASK_H
