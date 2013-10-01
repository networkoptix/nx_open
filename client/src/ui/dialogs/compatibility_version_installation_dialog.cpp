/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#include "compatibility_version_installation_dialog.h"

#include "ui_compatibility_version_installation_dialog.h"
#include "utils/applauncher_utils.h"


using namespace applauncher;

CompatibilityVersionInstallationDialog::CompatibilityVersionInstallationDialog( QWidget* parent )
:
    QDialog( parent ),
    ui( new Ui::CompatibilityVersionInstallationDialog() ),
    m_installationID( 0 ),
    m_state( State::init ),
    m_timerID( 0 )
{
    ui->setupUi(this);
}

CompatibilityVersionInstallationDialog::~CompatibilityVersionInstallationDialog()
{
}

static const int INSTALLATION_STATUS_CHECK_PERIOD_MS = 500;

void CompatibilityVersionInstallationDialog::onTimer( const quint64& /*timerID*/ )
{
    //NOTE this method is called from TimerManager thread (it's never GUI thread)
    m_timerID = 0;

    switch( m_state )
    {
        case State::init:
        {
            //starting installation
            api::ResultType::Value result = startInstallation( m_versionToInstall, &m_installationID );
            if( result != api::ResultType::ok )
            {
                m_state = State::failed;
                QMetaObject::invokeMethod( this, "onInstallationFailed", Qt::QueuedConnection, Q_ARG(decltype(result), result) );
            }
            else
            {
                m_state = State::installing;
                m_timerID = TimerManager::instance()->addTimer(this, INSTALLATION_STATUS_CHECK_PERIOD_MS);
            }
            break;
        }

        case State::installing:
        {
            //checking installation status
            api::InstallationStatus::Value status = api::InstallationStatus::unknown;
            float installationProgress = 0;
            const api::ResultType::Value result = getInstallationStatus( m_installationID, &status, &installationProgress );
            if( result != api::ResultType::ok )
            {
                m_state = State::failed;
                QMetaObject::invokeMethod( this, "onInstallationFailed", Qt::QueuedConnection, Q_ARG(int, result) );
            }
            else if( status == api::InstallationStatus::inProgress )
            {
                QMetaObject::invokeMethod( this, "updateInstallationProgress", Qt::QueuedConnection, Q_ARG(float, installationProgress) );
                m_timerID = TimerManager::instance()->addTimer(this, INSTALLATION_STATUS_CHECK_PERIOD_MS);
            }
            else if( status == api::InstallationStatus::failed )
            {
                m_state = State::failed;
                //installation failed
                QMetaObject::invokeMethod( this, "onInstallationFailed", Qt::QueuedConnection, Q_ARG(int, result) );
            }
            else if( status == api::InstallationStatus::success )
            {
                m_state = State::succeeded;
                QMetaObject::invokeMethod( this, "updateInstallationProgress", Qt::QueuedConnection, Q_ARG(float, installationProgress) );
                QMetaObject::invokeMethod( this, "onInstallationSucceeded", Qt::QueuedConnection );
            }
            else if( status == api::InstallationStatus::cancelled )
            {
                assert( false );
            }
            break;
        }

        case State::cancelRequested:
        {
            //starting cancellation process
            api::ResultType::Value result = applauncher::cancelInstallation( m_installationID );
            if( result == api::ResultType::ok )
            {
                m_state = State::cancelling;
                m_timerID = TimerManager::instance()->addTimer(this, INSTALLATION_STATUS_CHECK_PERIOD_MS);
            }
            break;
        }

        case State::cancelling:
        {
            //checking cancellation status
            api::InstallationStatus::Value status = api::InstallationStatus::unknown;
            float installationProgress = 0;
            const api::ResultType::Value result = getInstallationStatus( m_installationID, &status, &installationProgress );
            if( result != api::ResultType::ok )
            {
                m_state = State::failed;
                QMetaObject::invokeMethod( this, "onInstallationFailed", Qt::QueuedConnection, Q_ARG(int, result) );
            }
            else if( status == api::InstallationStatus::cancelInProgress )
            {
                QMetaObject::invokeMethod( this, "updateInstallationProgress", Qt::QueuedConnection, Q_ARG(float, installationProgress) );
                m_timerID = TimerManager::instance()->addTimer(this, INSTALLATION_STATUS_CHECK_PERIOD_MS);
            }
            else if( status == api::InstallationStatus::failed )
            {
                m_state = State::failed;
                //installation failed
                QMetaObject::invokeMethod( this, "onInstallationFailed", Qt::QueuedConnection, Q_ARG(int, result) );
            }
            else if( status == api::InstallationStatus::success )
            {
                assert( false );
            }
            else if( status == api::InstallationStatus::cancelled )
            {
                m_state = State::succeeded;
                QMetaObject::invokeMethod( this, "onCancelDone", Qt::QueuedConnection );
            }
            break;
        }

        case State::succeeded:
        case State::failed:
        case State::cancelled:
            break;
    }
}

void CompatibilityVersionInstallationDialog::setVersionToInstall( const QnSoftwareVersion& version )
{
    m_versionToInstall = version;
}

bool CompatibilityVersionInstallationDialog::installationSucceeded() const
{
    return m_state == State::succeeded;
}

void CompatibilityVersionInstallationDialog::reject()
{
    //TODO/IMPL synchronization with onTimer
    switch( m_state )
    {
        case State::installing:
            //starting cancelling installation
            m_state = State::cancelRequested;
            //disabling "Cancel" button
            ui->buttonBox->button( QDialogButtonBox::Cancel )->setDisabled(true);
            m_timerID = TimerManager::instance()->addTimer(this, 0);
            break;

        case State::cancelRequested:
        case State::cancelling:
            assert( false );
            return;

        case State::succeeded:
        case State::failed:
        case State::cancelled:
        default:
            QDialog::reject();
            return;
    }
}

void CompatibilityVersionInstallationDialog::showEvent(QShowEvent* event)
{
    //starting installation
    launchInstallation();

    QDialog::showEvent(event);
}

void CompatibilityVersionInstallationDialog::hideEvent(QHideEvent* event)
{
    //TODO/IMPL cancelling as soon as possible
    QDialog::hideEvent(event);
}

void CompatibilityVersionInstallationDialog::launchInstallation()
{
    m_state = State::init;
    m_timerID = TimerManager::instance()->addTimer( this, 0 );  //calling applauncher from non-GUI thread
    ui->progressBar->setValue( 0 );
    setWindowTitle( tr("Installing version %1").arg(m_versionToInstall.toString(QnSoftwareVersion::MinorFormat)) );
    ui->buttonBox->setStandardButtons( QDialogButtonBox::Cancel );
}

void CompatibilityVersionInstallationDialog::onInstallationFailed( int resultInt )
{
    const api::ResultType::Value result = static_cast<api::ResultType::Value>(resultInt);

    //TODO/IMPL
        //setting error message
    setWindowTitle( tr("Installation failed") );
    
    ui->buttonBox->button( QDialogButtonBox::Cancel )->setDisabled(false);
}

void CompatibilityVersionInstallationDialog::onInstallationSucceeded()
{
    //TODO/IMPL changing "changing Cancel button to OK"
    setWindowTitle( tr("Installation completed") );
    ui->buttonBox->setStandardButtons( QDialogButtonBox::Ok );
}

void CompatibilityVersionInstallationDialog::updateInstallationProgress( float progress )
{
    //TODO/IMPL updating progress bar
    if( progress < 0.0 )
        ;//TODO/IMPL unknown progress
    else
        ui->progressBar->setValue( (int)progress );
}

void CompatibilityVersionInstallationDialog::onCancelDone()
{
    //TODO/IMPL
        //setting error message
    setWindowTitle( tr("Installation has been cancelled") );

    ui->buttonBox->button( QDialogButtonBox::Cancel )->setDisabled(false);

    //QDialog::reject();
}
