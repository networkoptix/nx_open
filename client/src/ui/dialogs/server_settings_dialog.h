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

    typedef std::function<void()> OkCallback;
    
    /// @brief Prevents wrong using of dialog
    static void showNonModal(const QnMediaServerResourcePtr &server
        , const OkCallback &callback
        , QWidget *parent = nullptr);

private:
    QnServerSettingsDialog(const QnMediaServerResourcePtr &server
        , const OkCallback &callback
        , QWidget *parent);

    virtual ~QnServerSettingsDialog();

    void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    Q_DISABLE_COPY(QnServerSettingsDialog)

    const OkCallback m_onOkClickedCallback;
    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};
