#pragma once

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/dialog.h>

namespace Ui { class ExportSettingsDialog; }

class QnMediaResourceWidget;
class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

/**
 * Dialog with generic video export settings. Can be opened from the timeline context menu - this
 * is the default mode. Requires to have a selected widget. Also supports custom mode, when only
 * camera is provided (e.g. bookmark or audit trail export). Valid time period must be provided in
 * both cases.
 * Dialog will start and control the export process.
 * Export will be automatically cancelled if the provided resource is removed from the resource
 * pool (e.g. if reconnect was cancelled).
 */
class ExportSettingsDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    /** Default mode. Will have both "Single camera" and "Layout" tabs. */
    ExportSettingsDialog(QnMediaResourceWidget* widget,
        const QnTimePeriod& timePeriod,
        QWidget* parent = nullptr);
    ExportSettingsDialog(const QnVirtualCameraResourcePtr& camera,
        const QnTimePeriod& timePeriod,
        QWidget* parent = nullptr);
    virtual ~ExportSettingsDialog() override;

private:
    ExportSettingsDialog(const QnTimePeriod& timePeriod, QWidget* parent = nullptr);

private:
    class Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::ExportSettingsDialog> ui;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
