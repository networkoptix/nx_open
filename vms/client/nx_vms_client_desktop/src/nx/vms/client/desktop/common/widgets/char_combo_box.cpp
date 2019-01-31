#include "char_combo_box.h"
#include "private/char_combo_box_p.h"

namespace nx::vms::client::desktop {

CharComboBox::CharComboBox(QWidget* parent): base_type(parent)
{
    setView(new CharComboBoxListView(this));
}

} // namespace nx::vms::client::desktop
