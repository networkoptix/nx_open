#pragma once

#include <ui/dialogs/common/dialog.h>
#include <nx/vms/api/data/software_version.h>

namespace Ui {
class ApplauncherControlDialog;
}

namespace nx::vms::client::desktop {
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
} // namespace nx::vms::client::desktop
