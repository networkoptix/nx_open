#pragma once

#include <ui/dialogs/common/dialog.h>

namespace nx {
namespace client {
namespace ui {
namespace dialogs {

class QnApplauncherControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    QnApplauncherControlDialog(QWidget* parent = nullptr);

};

} // namespace dialogs
} // namespace ui
} // namespace client
} // namespace nx
