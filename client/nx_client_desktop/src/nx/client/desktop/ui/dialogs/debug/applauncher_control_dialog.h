#pragma once

#include <ui/dialogs/common/dialog.h>

namespace Ui {
class ApplauncherControlDialog;
}

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class QnApplauncherControlDialog: public QnDialog
{
    using base_type = QnDialog;
public:
    QnApplauncherControlDialog(QWidget* parent = nullptr);

private:
    QScopedPointer<Ui::ApplauncherControlDialog> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx