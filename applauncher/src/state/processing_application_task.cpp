////////////////////////////////////////////////////////////
// 14 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "processing_application_task.h"

#include <QApplication>
#include <QFinalState>

#include <utils/common/log.h>

#include "custom_transition.h"
#include "state/analyzing_stop_task_in_queue.h"
#include "state/download_setup.h"
#include "state/downloading.h"
#include "state/user_chooses_what_to_launch.h"
#include "state/critical_error_message.h"
#include "state/installing_application.h"
#include "state/launching_application.h"
#include "../launcher_common_data.h"


ProcessingApplicationTask::ProcessingApplicationTask(
    QState* const parent,
    QSettings* const settings,
    InstallationManager* const installationManager,
    LauncherCommonData* const fsmSharedData,
    BlockingQueue<QSharedPointer<applauncher::api::BaseTask> >* const taskQueue )
:
    QState( parent ),
    m_settings( settings ),
    m_installationManager( installationManager ),
    m_fsmSharedData( fsmSharedData ),
    m_taskQueue( taskQueue )
{
    initFsm();
}

void ProcessingApplicationTask::onEntry( QEvent* /*event*/ )
{
    NX_LOG( QString::fromLatin1("ProcessingApplicationTask entered"), cl_logDEBUG1 );
}

void ProcessingApplicationTask::onExit( QEvent* /*event*/ )
{
    NX_LOG( QString::fromLatin1("ProcessingApplicationTask exited"), cl_logDEBUG1 );
}

void ProcessingApplicationTask::initFsm()
{
    //creating states
    AnalyzingTopTaskInQueue* analyzingTopTaskInQueue = new AnalyzingTopTaskInQueue(
        this,
        m_fsmSharedData,
        m_taskQueue );
    setInitialState( analyzingTopTaskInQueue );

    DownloadSetup* downloadSetup = new DownloadSetup( this );

    Downloading* downloading = new Downloading( this );

    UserChoosesWhatToLaunch* userChoosesWhatToLaunch = new UserChoosesWhatToLaunch( this );

    CriticalErrorMessage* criticalErrorMessage = new CriticalErrorMessage( this );

    InstallingApplication* installingApplication = new InstallingApplication( this, *m_fsmSharedData, m_installationManager );

    LaunchingApplication* launchingApplication = new LaunchingApplication(
        this,
        *m_fsmSharedData,
        *m_installationManager,
        m_settings );

    QFinalState* finalState = new QFinalState( this );


    //creating transitions
        //from analyzingTopTaskInQueue
    {
        ConditionalSignalTransition* tran = new ConditionalSignalTransition(
            analyzingTopTaskInQueue,
            SIGNAL(done()),
            analyzingTopTaskInQueue,
            launchingApplication );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "currentTaskType", (int)applauncher::api::TaskType::startApplication) );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "isRequiredVersionInstalled", true) );
        analyzingTopTaskInQueue->addTransition( tran );
    }

    {
        ConditionalSignalTransition* tran = new ConditionalSignalTransition(
            analyzingTopTaskInQueue,
            SIGNAL(done()),
            analyzingTopTaskInQueue,
            downloadSetup );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "currentTaskType", (int)applauncher::api::TaskType::startApplication) );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "isRequiredVersionInstalled", false) );
        analyzingTopTaskInQueue->addTransition( tran );
    }

    {
        ConditionalSignalTransition* tran = new ConditionalSignalTransition(
            analyzingTopTaskInQueue,
            SIGNAL(done()),
            analyzingTopTaskInQueue,
            finalState );
        tran->addCondition( new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "currentTaskType", (int)applauncher::api::TaskType::quit) );
        connect( tran, SIGNAL(triggered()), QApplication::instance(), SLOT(quit()) );
        analyzingTopTaskInQueue->addTransition( tran );
    }

        //from downloadSetup
    downloadSetup->addTransition( downloadSetup, SIGNAL(ok()), downloading );

        //from downloading
    downloading->addTransition( downloading, SIGNAL(succeeded()), installingApplication );

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            downloading,
            SIGNAL(failed()),
            downloading,
            userChoosesWhatToLaunch,
            new ObjectPropertyGreaterConditionHelper<int>::CondType(m_installationManager, "count", 0) );
        connect( tran, SIGNAL(triggered()), downloading, SLOT(prepareResultMessage()) );
        downloading->addTransition( tran );
    }

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            downloading,
            SIGNAL(failed()),
            downloading,
            criticalErrorMessage,
            new ObjectPropertyEqualConditionHelper<int>::CondType(m_installationManager, "count", 0) );
        connect( tran, SIGNAL(triggered()), downloading, SLOT(prepareResultMessage()) );
        downloading->addTransition( tran );
    }

        //from userChoosesWhatToLaunch
    userChoosesWhatToLaunch->addTransition( userChoosesWhatToLaunch, SIGNAL(cancelled()), finalState );

    userChoosesWhatToLaunch->addTransition(
        new ConditionalSignalTransition(
            userChoosesWhatToLaunch,
            SIGNAL(ok()),
            userChoosesWhatToLaunch,
            launchingApplication,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "isRequiredVersionInstalled", true) ) );

    userChoosesWhatToLaunch->addTransition(
        new ConditionalSignalTransition(
            userChoosesWhatToLaunch,
            SIGNAL(ok()),
            userChoosesWhatToLaunch,
            downloadSetup,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "isRequiredVersionInstalled", false) ) );

        //from criticalErrorMessage
    criticalErrorMessage->addTransition( criticalErrorMessage, SIGNAL(ok()), finalState );

        //from installingApplication
    installingApplication->addTransition( installingApplication, SIGNAL(succeeded()), launchingApplication );

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            installingApplication,
            SIGNAL(failed()),
            installingApplication,
            userChoosesWhatToLaunch,
            new ObjectPropertyGreaterConditionHelper<int>::CondType(m_installationManager, "count", 0) );
        connect( tran, SIGNAL(triggered()), installingApplication, SLOT(prepareResultMessage()) );
        installingApplication->addTransition( tran );
    }

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            installingApplication,
            SIGNAL(failed()),
            installingApplication,
            criticalErrorMessage,
            new ObjectPropertyEqualConditionHelper<int>::CondType(m_installationManager, "count", 0) );
        connect( tran, SIGNAL(triggered()), installingApplication, SLOT(prepareResultMessage()) );
        installingApplication->addTransition( tran );
    }


        //from launchingApplication
    launchingApplication->addTransition( launchingApplication, SIGNAL(succeeded()), finalState );

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            launchingApplication,
            SIGNAL(failed()),
            launchingApplication,
            userChoosesWhatToLaunch,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "areThereAnyVersionInstalledBesidesJustTriedOne", true) );
        connect( tran, SIGNAL(triggered()), launchingApplication, SLOT(prepareResultMessage()) );
        launchingApplication->addTransition( tran );
    }

    {
        QAbstractTransition* tran = new ConditionalSignalTransition(
            launchingApplication,
            SIGNAL(failed()),
            launchingApplication,
            criticalErrorMessage,
            new ObjectPropertyEqualConditionHelper<bool>::CondType(m_fsmSharedData, "areThereAnyVersionInstalledBesidesJustTriedOne", false) );
        connect( tran, SIGNAL(triggered()), launchingApplication, SLOT(prepareResultMessage()) );
        launchingApplication->addTransition( tran );
    }
}
