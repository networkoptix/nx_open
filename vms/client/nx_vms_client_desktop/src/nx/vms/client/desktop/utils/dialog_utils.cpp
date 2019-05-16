#include "dialog_utils.h"

#include <QtWidgets/QDialog>

namespace nx::vms::client::desktop {

void DialogUtils::setDialogParent(QDialog* dialog, QWidget* parent)
{
    // We have to keep flags for dialogs if we don't want to place it
    // as widget on the parent
    const auto flags = dialog->windowFlags();
    dialog->setParent(parent);
    dialog->setWindowFlags(flags);
}

} // namespace nx::vms::client::desktop
