#include "session_aware_dialog.h"

/************************************************************************/
/* QnSessionAwareButtonBoxDialog                             */
/************************************************************************/
QnSessionAwareButtonBoxDialog::QnSessionAwareButtonBoxDialog(QWidget *parent,
    Qt::WindowFlags windowFlags /* = 0*/)
    :
    base_type(parent, windowFlags)
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
QnSessionAwareTabbedDialog::QnSessionAwareTabbedDialog(QWidget *parent,
    Qt::WindowFlags windowFlags /* = 0*/ )
    :
    base_type(parent, windowFlags)
    , QnSessionAwareDelegate(parent)
{}

bool QnSessionAwareTabbedDialog::tryClose( bool force ) {
    if (force)
        return forcefullyClose();

    if (!canDiscardChanges())
        return false;

    if (isHidden() || !hasChanges())
        return true;

    switch (showConfirmationDialog())
    {
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


QDialogButtonBox::StandardButton QnSessionAwareTabbedDialog::showConfirmationDialog()
{
    QStringList details;
    for(const Page &page: modifiedPages())
        details << lit("* %1").arg(page.title);

    static const auto kDelimiter = L'\n';
    const auto extraMessage = tr("Unsaved changes:") + kDelimiter + details.join(kDelimiter);

    return QnMessageBox::question(this, tr("Save changes before exit?"), extraMessage,
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
        QDialogButtonBox::Yes);
}
