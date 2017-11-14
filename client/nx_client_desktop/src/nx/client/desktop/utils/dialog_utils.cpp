#include "dialog_utils.h"

namespace nx {
namespace utils {

void setDialogParent(QDialog* dialog, QWidget* parent)
{
    // We have to keep flags for dialogs if we don't want to place it
    // as widget on the parent
    const auto flags = dialog->windowFlags();
    dialog->setParent(nullptr); // Workaround for QTBUG-34767. In some cases we have to reset
                                // parent and specifiy the new one, overwise window order
                                // will be wrong
    dialog->setParent(parent);
    dialog->setWindowFlags(flags);
}

} // namespace utils
} // namespace nx
