#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnSystemAdministrationDialog;
}

class QnAbstractPreferencesWidget;

class QnSystemAdministrationDialog : public QnButtonBoxDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnButtonBoxDialog base_type;
public:
    enum DialogPage {
        ServerPage,
        LicensesPage,
        UpdatesPage,

        PageCount
    };

    QnSystemAdministrationDialog(QWidget *parent = 0);

    DialogPage currentPage() const;
    void setCurrentPage(DialogPage page);

    virtual void reject() override;
    virtual void accept() override;
private:
    void updateFromSettings();
    void submitToSettings();

private:
    QScopedPointer<Ui::QnSystemAdministrationDialog> ui;

    QMap<DialogPage, QnAbstractPreferencesWidget*> m_pages;
};

#endif // UPDATE_DIALOG_H
