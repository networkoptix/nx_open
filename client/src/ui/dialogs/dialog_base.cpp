
#include "dialog_base.h"


QnDialogBase::QnDialogBase(QWidget * parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) 
{
    cancelDrag();
}

void QnDialogBase::show(QnDialogBase *dialog)
{
    if (!dialog)
        return;

    dialog->show();
}

void QnDialogBase::show()
{
    QDialog::show();
    cancelDrag();
}

void QnDialogBase::cancelDrag()
{
    /// Cancels any drag event 
    grabMouse();
    releaseMouse();
}