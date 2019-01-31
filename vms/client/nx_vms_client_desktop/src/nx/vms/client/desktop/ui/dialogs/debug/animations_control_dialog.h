#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::desktop {
namespace ui {

class AnimationsControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    AnimationsControlDialog(QWidget* parent);

};

} // namespace ui
} // namespace nx::vms::client::desktop
