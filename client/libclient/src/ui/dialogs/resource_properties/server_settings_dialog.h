#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnServerSettingsWidget;
class QnStorageAnalyticsWidget;
class QnStorageConfigWidget;

namespace Ui {
    class ServerSettingsDialog;
}

class QnServerSettingsDialog: public QnWorkbenchStateDependentTabbedDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentTabbedDialog base_type;

public:
    enum DialogPage
    {
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

    virtual QDialogButtonBox::StandardButton showConfirmationDialog() override;

private:
    void updateWebPageButton();

private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QnServerSettingsWidget* m_generalPage;
    QnStorageAnalyticsWidget* m_statisticsPage;
    QnStorageConfigWidget* m_storagesPage;
    QPushButton* m_webPageButton;
};
