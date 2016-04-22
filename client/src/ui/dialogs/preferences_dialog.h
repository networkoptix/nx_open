#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/common/generic_tabbed_dialog.h>

namespace Ui {
    class PreferencesDialog;
}

class QnGeneralPreferencesWidget;
class QnLookAndFeelPreferencesWidget;

class QnPreferencesDialog: public QnGenericTabbedDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnGenericTabbedDialog base_type;
public:
    enum DialogPage {
        GeneralPage,
        LookAndFeelPage,
        RecordingPage,
        NotificationsPage,

        PageCount
    };

    QnPreferencesDialog(QWidget *parent = 0);
    ~QnPreferencesDialog();

protected:

    virtual void applyChanges() override;

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QnGeneralPreferencesWidget* m_generalWidget;
    QnLookAndFeelPreferencesWidget* m_lookAndFeelWidget;
};

#endif // QN_PREFERENCES_DIALOG_H
