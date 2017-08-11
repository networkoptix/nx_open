#include "export_settings_dialog.h"
#include "ui_export_settings_dialog.h"

#include <nx/client/desktop/export/ui/dialogs/private/export_settings_dialog_p.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

ExportSettingsDialog::ExportSettingsDialog(QWidget *parent) :
    base_type(parent),
    d(new Private()),
    ui(new Ui::ExportSettingsDialog)
{
    ui->setupUi(this);
}

ExportSettingsDialog::~ExportSettingsDialog()
{
}


} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
