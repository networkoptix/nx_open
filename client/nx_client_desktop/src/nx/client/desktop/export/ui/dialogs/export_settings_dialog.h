#pragma once

#include <QtWidgets/QDialog>

#include <ui/dialogs/common/dialog.h>

namespace Ui { class ExportSettingsDialog; }

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class ExportSettingsDialogPrivate;

class ExportSettingsDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    explicit ExportSettingsDialog(QWidget* parent = nullptr);
    virtual ~ExportSettingsDialog() override;

private:
    class Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::ExportSettingsDialog> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
