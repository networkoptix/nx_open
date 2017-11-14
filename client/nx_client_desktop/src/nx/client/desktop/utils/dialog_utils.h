#pragma once

class QDialog;
class QWidget;

namespace nx {
namespace utils {

/** Sets parent for dialog and preservs window flags. Overwise, dialog will be placed as widget */
void setDialogParent(QDialog* dialog, QWidget* parent);

} // namespace utils
} // namespace nx

