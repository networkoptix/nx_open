#pragma once

#include <QtCore/QScopedPointer>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/generic_tabbed_dialog.h>

namespace Ui
{
    class LocalSettingsDialog;
}

class QLabel;
class QnAdvancedSettingsWidget;
class QnLookAndFeelPreferencesWidget;

class QnLocalSettingsDialog : public QnGenericTabbedDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QnGenericTabbedDialog;
public:
    enum DialogPage
    {
        GeneralPage,
        LookAndFeelPage,
        RecordingPage,
        NotificationsPage,
        AdvancedPage,

        PageCount
    };

    QnLocalSettingsDialog(QWidget *parent = 0);
    ~QnLocalSettingsDialog();

protected:
    virtual bool canApplyChanges() const override;
    virtual void applyChanges() override;
    virtual void updateButtonBox() override;

    virtual void accept() override;
private:
    bool isRestartRequired() const;

    void addRestartLabel();

private:
    Q_DISABLE_COPY(QnLocalSettingsDialog)

    QScopedPointer<Ui::LocalSettingsDialog> ui;
    QnLookAndFeelPreferencesWidget* m_lookAndFeelWidget;
    QnAdvancedSettingsWidget* m_advancedSettingsWidget;
    QLabel* m_restartLabel;
};
