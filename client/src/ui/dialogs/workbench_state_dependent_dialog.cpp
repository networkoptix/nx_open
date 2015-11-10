#include "workbench_state_dependent_dialog.h"

/************************************************************************/
/* QnWorkbenchStateDependentButtonBoxDialog                             */
/************************************************************************/
QnWorkbenchStateDependentButtonBoxDialog::QnWorkbenchStateDependentButtonBoxDialog(QWidget *parent /* = NULL*/, Qt::WindowFlags windowFlags /* = 0*/)
    : base_type(parent, windowFlags)
    , QnWorkbenchStateDelegate(parent)
{
}

bool QnWorkbenchStateDependentButtonBoxDialog::tryClose(bool force) {
    base_type::reject();
    if (force)
        base_type::hide();
    return true;
}

void QnWorkbenchStateDependentButtonBoxDialog::forcedUpdate() {
    retranslateUi();
    tryClose(true);
}

/************************************************************************/
/* QnWorkbenchStateDependentTabbedDialog                                */
/************************************************************************/
QnWorkbenchStateDependentTabbedDialog::QnWorkbenchStateDependentTabbedDialog( QWidget *parent /* = NULL*/, Qt::WindowFlags windowFlags /* = 0*/ )
    : base_type(parent, windowFlags)
    , QnWorkbenchStateDelegate(parent)
{}

bool QnWorkbenchStateDependentTabbedDialog::tryClose( bool force ) {
    if (force)
        return forcefullyClose();

    if (!canDiscardChanges())
        return false;

    if (isHidden() || !hasChanges())
        return true;

    switch (showConfirmationDialog()) {
    case QMessageBox::Yes:
        if (!canApplyChanges())
            return false;   // e.g. cancel was pressed in the confirmation dialog

        applyChanges();
        break;
    case QMessageBox::No:
        loadDataToUi();
        break;
    default:
        return false;   // Cancel was pressed
    }

    return true;
}


void QnWorkbenchStateDependentTabbedDialog::forcedUpdate() {
    loadDataToUi();
}


QMessageBox::StandardButton QnWorkbenchStateDependentTabbedDialog::showConfirmationDialog() {
    auto confirmMessageText = [this]{
        QStringList details;
        for(const Page &page: modifiedPages())
            details << tr("* %1").arg(page.title);
        return tr("Unsaved changes will be lost. Save the following pages?") + L'\n' + details.join(L'\n');
    };

    return QMessageBox::question(this,
        tr("Confirm exit"),
        confirmMessageText(),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Cancel);
}
