#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/button_box_dialog.h>

#include <utils/common/connective.h>

namespace Ui { class ExportSettingsDialog; }

class QnMediaResourceWidget;
class QnTimePeriod;
struct QnCameraBookmark;

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;
struct ExportLayoutSettings;

/**
 * Dialog with generic video export settings. Can be opened from the timeline context menu - this
 * is the default mode. Requires to have a selected widget. Also supports custom mode, when only
 * camera is provided (e.g. bookmark or audit trail export). Valid time period must be provided in
 * both cases.
 */
class ExportSettingsDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    enum class Mode
    {
        Media,
        Layout,
    };

    /** Default mode. Will have both "Single camera" and "Layout" tabs. */
    ExportSettingsDialog(QnMediaResourceWidget* widget,
        const QnTimePeriod& timePeriod,
        QWidget* parent = nullptr);

    /** Bookmark mode. Will have only "Single camera" tab. */
    ExportSettingsDialog(QnMediaResourceWidget* widget,
        const QnCameraBookmark& bookmark,
        QWidget* parent = nullptr);

    virtual ~ExportSettingsDialog() override;

    Mode mode() const;

    ExportMediaSettings exportMediaSettings() const;
    ExportLayoutSettings exportLayoutSettings() const;

    virtual void accept() override;

private:
    ExportSettingsDialog(const QnTimePeriod& timePeriod, const QnCameraBookmark& bookmark,
        QWidget* parent = nullptr);

    void setMediaResourceWidget(QnMediaResourceWidget* widget);
    void setupSettingsButtons();
    void updateSettingsWidgets();
    void updateMode();
    void updateAlerts(Mode mode, const QStringList& texts);

private:
    class Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::ExportSettingsDialog> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
