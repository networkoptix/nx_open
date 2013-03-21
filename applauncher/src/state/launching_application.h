////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LAUNCHING_APPLICATION_H
#define LAUNCHING_APPLICATION_H

#include <QSettings>
#include <QState>


class LauncherCommonData;
class InstallationManager;

class LaunchingApplication
:
    public QState
{
    Q_OBJECT

public:
    LaunchingApplication(
        QState* const parent,
        const LauncherCommonData& launcherCommonData,
        const InstallationManager& installationManager,
        QSettings* const settings );

public slots:
    void prepareResultMessage();

signals:
    //!Application has been launched
    void succeeded();
    //!Failed to launch application
    void failed();

protected:
    virtual void onEntry( QEvent* _event ) override;

private:
    const LauncherCommonData& m_launcherCommonData;
    const InstallationManager& m_installationManager;
    QSettings* const m_settings;
};

#endif  //LAUNCHING_APPLICATION_H
