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
    QnApplauncherControlDialog(
        QWidget* parent = nullptr,
        const api::SoftwareVersion& engineVersion = api::SoftwareVersion());

private:
    QScopedPointer<Ui::ApplauncherControlDialog> ui;
    const api::SoftwareVersion m_engineVersion;
};

} // namespace ui
} // namespace nx::vms::client::desktop
