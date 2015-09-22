#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnWorkbenchStateDelegate;

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
   
    /// @brief Prevents wrong using of dialog
    static void showNonModal(const QnMediaServerResourcePtr &server , QWidget *parent = nullptr);

    bool tryClose(bool force);

private:
    QnServerSettingsDialog(const QnMediaServerResourcePtr &server
        , QWidget *parent);

    virtual ~QnServerSettingsDialog();

    void accept() override;

private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};
