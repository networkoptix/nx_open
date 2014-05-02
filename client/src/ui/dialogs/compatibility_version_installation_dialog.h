/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
#define COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H

#include <mutex>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>
#include <QtWidgets/QWidget>

#include <utils/common/software_version.h>
#include <utils/common/stoppable.h>
#include <utils/common/timermanager.h>
#include <api/applauncher_api.h>


namespace Ui {
    class CompatibilityVersionInstallationDialog;
}

class CompatibilityVersionInstallationDialog
:
    public QDialog,
    public TimerEventHandler,
    public QnStoppable
{
    Q_OBJECT

public:
    CompatibilityVersionInstallationDialog( QWidget* parent = NULL );
    virtual ~CompatibilityVersionInstallationDialog();

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID ) override;
    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setVersionToInstall( const QnSoftwareVersion& version );

    bool installationSucceeded() const;

public slots:
    virtual void reject() override;

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private:
    // TODO: #Elric #enum
    enum class State
    {
        init,
        installing,
        cancelRequested,
        cancelling,
        succeeded,
        failed,
        cancelled
    };

    QScopedPointer<Ui::CompatibilityVersionInstallationDialog> ui;
    QnSoftwareVersion m_versionToInstall;
    unsigned int m_installationID;
    State m_state;
    quint64 m_timerID;
    mutable std::mutex m_mutex;
    bool m_terminated;

    void launchInstallation();

private slots:
    void onInstallationFailed( int resultInt );
    void onCancelFailed( int resultInt );
    void onInstallationSucceeded();
    void updateInstallationProgress( float progress );
    void onCancelDone();
};

#endif  //COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
