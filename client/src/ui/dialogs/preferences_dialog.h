#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/generic_tabbed_dialog.h>
#include <ui_preferences_dialog.h>


class QnPreferencesDialog: public QnGenericTabbedDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnGenericTabbedDialog base_type;
public:
    enum DialogPage {
        GeneralPage,
        RecordingPage,
        NotificationsPage,

        PageCount
    };

    QnPreferencesDialog(QWidget *parent = 0);

    virtual void submitData() override;
private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
};

#endif // QN_PREFERENCES_DIALOG_H
