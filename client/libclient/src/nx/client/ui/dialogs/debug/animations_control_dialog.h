#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

class AnimationsControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    AnimationsControlDialog(QWidget* parent = nullptr);

};

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
