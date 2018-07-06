#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class AnimationsControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    AnimationsControlDialog(QWidget* parent);

};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
