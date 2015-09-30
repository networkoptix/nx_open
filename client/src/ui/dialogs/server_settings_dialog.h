#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnWorkbenchStateDelegate;
class QnServerSettingsWidget;
class QnRecordingStatisticsWidget;

namespace Ui {
    class ServerSettingsDialog;
}

class QnServerSettingsDialog: public QnGenericTabbedDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnGenericTabbedDialog base_type;

public:
    enum DialogPage {
        SettingsPage,
        StatisticsPage,

        PageCount
    };

    QnServerSettingsDialog(QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();
   
    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    virtual void loadData() override;

    virtual QString confirmMessageTitle() const override;
    virtual QString confirmMessageText() const override;
private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
    QnServerSettingsWidget* m_generalPage;
    QnRecordingStatisticsWidget* m_statisticsPage;
    QPushButton* m_webPageButton;
};
