#pragma once

class QDialog;
class QWidget;

namespace nx {
namespace client {
namespace desktop {

class DialogUtils
{
public:
    /** Sets parent for dialog and preservs window flags. Overwise, dialog will be placed as widget */
    static void setDialogParent(QDialog* dialog, QWidget* parent);
};

} // namespace desktop
} // namespace client
} // namespace nx

