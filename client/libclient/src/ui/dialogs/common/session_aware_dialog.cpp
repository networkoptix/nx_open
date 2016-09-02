#include "session_aware_dialog.h"

/************************************************************************/
/* QnSessionAwareButtonBoxDialog                             */
/************************************************************************/
QnSessionAwareButtonBoxDialog::QnSessionAwareButtonBoxDialog(QWidget *parent /* = NULL*/, Qt::WindowFlags windowFlags /* = 0*/)
    : base_type(parent, windowFlags)
    , QnSessionAwareDelegate(parent)
{
}

bool QnSessionAwareButtonBoxDialog::tryClose(bool force) {
    base_type::reject();
    if (force)
        base_type::hide();
    return true;
}

void QnSessionAwareButtonBoxDialog::forcedUpdate() {
    retranslateUi();
    tryClose(true);
}

/************************************************************************/
/* QnSessionAwareTabbedDialog                                */
/************************************************************************/
QnSessionAwareTabbedDialog::QnSessionAwareTabbedDialog( QWidget *parent /* = NULL*/, Qt::WindowFlags windowFlags /* = 0*/ )
    : base_type(parent, windowFlags)
    , QnSessionAwareDelegate(parent)
{}

bool QnSessionAwareTabbedDialog::tryClose( bool force ) {
    if (force)
        return forcefullyClose();

    if (!canDiscardChanges())
        return false;

    if (isHidden() || !hasChanges())
        return true;

    switch (showConfirmationDialog()) {
    case QDialogButtonBox::Yes:
        if (!canApplyChanges())
            return false;   // e.g. cancel was pressed in the confirmation dialog

        applyChanges();
        break;
    case QDialogButtonBox::No:
        loadDataToUi();
        break;
    default:
        return false;   // Cancel was pressed
    }

    return true;
}


void QnSessionAwareTabbedDialog::forcedUpdate() {
    loadDataToUi();
}


QDialogButtonBox::StandardButton QnSessionAwareTabbedDialog::showConfirmationDialog() {
    auto confirmMessageText = [this]{
        QStringList details;
        for(const Page &page: modifiedPages())
            details << tr("* %1").arg(page.title);
        return tr("Unsaved changes will be lost. Save the following pages?") + L'\n' + details.join(L'\n');
    };

    return QnMessageBox::question(this,
        tr("Confirm exit"),
        confirmMessageText(),
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
        QDialogButtonBox::Cancel);
}
