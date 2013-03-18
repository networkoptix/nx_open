////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef INSTALLING_APPLICATION_H
#define INSTALLING_APPLICATION_H

#include <QState>


class LauncherCommonData;
class InstallationManager;

class InstallingApplication
:
    public QState
{
    Q_OBJECT

public:
    InstallingApplication(
        QState* const parent,
        const LauncherCommonData& launcherCommonData,
        InstallationManager* const installationManager );

signals:
    void succeeded();
    void failed();

protected:
    virtual void onEntry( QEvent* _event ) override;

private:
    const LauncherCommonData& m_launcherCommonData;
    InstallationManager* const m_installationManager;
};

#endif  //INSTALLING_APPLICATION_H
