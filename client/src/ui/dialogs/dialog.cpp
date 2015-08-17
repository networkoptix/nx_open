
#include "dialog.h"

namespace
{
    void cancelDrag(QDialog *dialog)
    {
        Q_ASSERT_X(dialog, "cancelDrag", "Dialog is null");
        /// Cancels any drag event 
        dialog->grabMouse();
        dialog->releaseMouse();
    }
}

QnDialog::QnDialog(QWidget * parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) 
{
    cancelDrag(this);
}

void QnDialog::show(QDialog *dialog)
{
    Q_ASSERT_X(dialog, "QnDialog::show", "Dialog is null");

    if (!dialog)
        return;

    dialog->show();
    cancelDrag(dialog);
}

void QnDialog::show()
{
    show(this); /// Calls static member of QnDialog
}
