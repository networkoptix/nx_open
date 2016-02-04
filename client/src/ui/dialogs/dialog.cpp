
#include "dialog.h"

#include <ui/workaround/cancel_drag.h>

QnDialog::QnDialog(QWidget * parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
{
    cancelDrag(this);
}

void QnDialog::show(QDialog *dialog)
{
    Q_ASSERT_X(dialog, Q_FUNC_INFO, "Dialog is null");

    if (!dialog)
        return;

    dialog->show();
    cancelDrag(dialog);
}

void QnDialog::show()
{
    show(this); /// Calls static member of QnDialog
}

int QnDialog::exec()
{
    cancelDrag(parentWidget());
    return base_type::exec();
}
