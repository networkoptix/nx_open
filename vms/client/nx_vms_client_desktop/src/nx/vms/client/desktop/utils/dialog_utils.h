#pragma once

class QDialog;
class QWidget;

namespace nx::vms::client::desktop {

class DialogUtils
{
public:
    /** Sets parent for dialog and preservs window flags. Overwise, dialog will be placed as widget */
    static void setDialogParent(QDialog* dialog, QWidget* parent);
};

} // namespace nx::vms::client::desktop

