#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <utils/common/connective.h>

#include <nx/client/desktop/common/utils/filesystem.h>

namespace Ui { class ExportSettingsDialog; }

class QnMediaResourceWidget;
class QnTimePeriod;
class QnWorkbenchContext;
struct QnCameraBookmark;
struct QnLayoutItemData;

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
    Q_ENUMS(Mode)
    using base_type = QnButtonBoxDialog;

public:
    enum class Mode
    {
        Media,
        Layout,
    };

    using FileNameValidator = std::function<bool (const Filename& fileName, bool quiet)>;
    ExportSettingsDialog(const QnTimePeriod& timePeriod,
        const QnCameraBookmark& bookmark,
        FileNameValidator isFileNameValid,
        QWidget* parent = nullptr);

    virtual ~ExportSettingsDialog() override;

    Mode mode() const;

    ExportMediaSettings exportMediaSettings() const;
    ExportLayoutSettings exportLayoutSettings() const;

    virtual void accept() override;

    // Making this methods private causes pointless code bloat
    void disableTab(Mode mode, const QString& reason);
    void hideTab(Mode mode);
    void setLayout(const QnLayoutResourcePtr& layout);
    void setMediaParams(const QnMediaResourcePtr& mediaResource, const QnLayoutItemData& itemData,
        QnWorkbenchContext* context);
private:

    void setupSettingsButtons();
    void updateSettingsWidgets();
    void updateMode();
    void updateTabWidgetSize();
    void updateAlerts(Mode mode, const QStringList& weakAlerts, const QStringList& severeAlerts);
    static void updateAlertsInternal(QLayout* layout, const QStringList& texts, bool severe);
    void updateTranscodingWidgets(bool transcodingIsAllowed);

    Filename suggestedFileName(const Filename& baseName) const;

private:
    class Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::ExportSettingsDialog> ui;
    FileNameValidator isFileNameValid;
};

} // namespace desktop
} // namespace client
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::client::desktop::ExportSettingsDialog::Mode, (metatype)(lexical))
