#include "char_combo_box.h"
#include "private/char_combo_box_p.h"

namespace nx {
namespace client {
namespace desktop {

CharComboBox::CharComboBox(QWidget* parent): base_type(parent)
{
    setView(new CharComboBoxListView(this));
}

} // namespace desktop
} // namespace client
} // namespace nx
