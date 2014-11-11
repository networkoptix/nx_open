#include "workbench_state_dependent_dialog.h"

QnWorkbenchStateDependentButtonBoxDialog::QnWorkbenchStateDependentButtonBoxDialog(QWidget *parent /*= NULL*/, Qt::WindowFlags windowFlags /*= 0*/):
    base_type(parent, windowFlags),
    QnWorkbenchStateDelegate(parent)
{
}

bool QnWorkbenchStateDependentButtonBoxDialog::tryClose(bool force) {
    base_type::reject();
    if (force)
        base_type::hide();
    return true;
}

void QnWorkbenchStateDependentButtonBoxDialog::forcedUpdate() {
    tryClose(true);
}


