#ifndef QN_PREFERENCES_DIALOG_H
#define QN_PREFERENCES_DIALOG_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialog>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/button_box_dialog.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>

class QnWorkbenchContext;
class QnClientSettings;

namespace Ui {
    class PreferencesDialog;
}

class QnPreferencesDialog: public QnButtonBoxDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    enum DialogPage {
        GeneralPage,
        RecordingPage,
        NotificationsPage,

        PageCount
    };

    QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnPreferencesDialog();

    DialogPage currentPage() const;
    void setCurrentPage(DialogPage page);

    virtual void accept() override;
private:
    void updateFromSettings();
    void submitToSettings();

private:
    Q_DISABLE_COPY(QnPreferencesDialog)

    QScopedPointer<Ui::PreferencesDialog> ui;
    QMap<DialogPage, QnAbstractPreferencesWidget*> m_pages;
};

#endif // QN_PREFERENCES_DIALOG_H
