#pragma once

#include <QtCore/QScopedPointer>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/generic_tabbed_dialog.h>

namespace Ui
{
    class LocalSettingsDialog;
}

class QnGeneralPreferencesWidget;
class QnLookAndFeelPreferencesWidget;

class QnLocalSettingsDialog : public QnGenericTabbedDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnGenericTabbedDialog base_type;
public:
    enum DialogPage
    {
        GeneralPage,
        LookAndFeelPage,
        RecordingPage,
        NotificationsPage,

        PageCount
    };

    QnLocalSettingsDialog(QWidget *parent = 0);
    ~QnLocalSettingsDialog();

protected:
    virtual void applyChanges() override;

private:
    Q_DISABLE_COPY(QnLocalSettingsDialog)

    QScopedPointer<Ui::LocalSettingsDialog> ui;
    QnGeneralPreferencesWidget* m_generalWidget;
    QnLookAndFeelPreferencesWidget* m_lookAndFeelWidget;
};
