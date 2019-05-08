#include "session_expired_dialog.h"

#include <ui/workbench/workbench_context.h>
#include <ui/dialogs/common/message_box.h>

void nx::vms::client::desktop::SessionExpiredDialog::exec(QnWorkbenchContext* context)
{

    QnMessageBox dialog(QnMessageBoxIcon::Warning, tr("Your session has expired"),
        tr("Session duration limit can be changed by the system administrators"),
        QDialogButtonBox::Ok, QDialogButtonBox::Ok, context->mainWindowWidget());
    dialog.exec();
}
