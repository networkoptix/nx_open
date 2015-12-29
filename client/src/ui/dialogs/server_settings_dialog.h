#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnWorkbenchStateDelegate;
class QnServerSettingsWidget;
class QnRecordingStatisticsWidget;
class QnStorageConfigWidget;

namespace Ui {
    class ServerSettingsDialog;
}

class QnServerSettingsDialog: public QnWorkbenchStateDependentTabbedDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentTabbedDialog base_type;

public:
    enum DialogPage {
        SettingsPage,
        StatisticsPage,
        StorageManagmentPage,

        PageCount
    };

    QnServerSettingsDialog(QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();
   
    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    virtual void retranslateUi() override;

    virtual QMessageBox::StandardButton showConfirmationDialog() override;
private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QnServerSettingsWidget* m_generalPage;
    QnRecordingStatisticsWidget* m_statisticsPage;
    QnStorageConfigWidget* m_storagesPage;
    QPushButton* m_webPageButton;
};
