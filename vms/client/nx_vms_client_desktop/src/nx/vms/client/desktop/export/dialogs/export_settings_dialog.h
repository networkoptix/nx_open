// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/common/widgets/message_bar.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/settings/types/export_mode.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class ExportSettingsDialog; }

class QnMediaResourceWidget;
struct QnTimePeriod;
class QnWorkbenchContext;
namespace nx::vms::common { struct CameraBookmark; }
using QnCameraBookmark = nx::vms::common::CameraBookmark;

namespace nx::vms::common { struct LayoutItemData; }

namespace nx::vms::client::desktop {

class SelectableTextButton;
class ExportPasswordWidget;
struct ExportMediaSettings;
struct ExportLayoutSettings;

/**
 * Dialog with generic video export settings. Can be opened from the timeline context menu - this
 * is the default mode. Requires to have a selected widget. Also supports custom mode, when only
 * camera is provided (e.g. bookmark or audit trail export). Valid time period must be provided in
 * both cases.
 */
class ExportSettingsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    using FileNameValidator = std::function<bool (const Filename& fileName, bool quiet)>;
    using BarDescs = std::vector<BarDescription>;
    ExportSettingsDialog(const QnTimePeriod& timePeriod,
        const QnCameraBookmark& bookmark,
        FileNameValidator isFileNameValid,
        const nx::core::Watermark& watermark,
        QWidget* parent = nullptr);

    virtual ~ExportSettingsDialog() override;

    /** Forcibly update the dialog data. */
    void forcedUpdate();

    ExportMode mode() const;
    ExportMediaSettings exportMediaSettings() const;
    ExportLayoutSettings exportLayoutSettings() const;

    QnMediaResourcePtr mediaResource() const;
    LayoutResourcePtr layout() const;

    virtual void accept() override;

    // Making this methods private causes pointless code bloat
    void disableTab(ExportMode mode, const QString& reason);
    void setLayout(const LayoutResourcePtr& layout);
    void setMediaParams(
        const QnMediaResourcePtr& mediaResource, const nx::vms::common::LayoutItemData& itemData);
    void setBookmarks(const QnCameraBookmarkList& bookmarks);

    virtual int exec() override;

private:
    SelectableTextButton* buttonForOverlayType(ExportOverlayType type);
    void setupSettingsButtons();
    void initSettingsWidgets();
    void updateTabWidgetSize();
    void updateMessageBars(ExportMode mode, const BarDescs& barDescriptions);
    void updateMessageBarsInternal(ExportMode mode, const BarDescs& barDescriptions);

    // Updates state of widgets according to internal state
    void updateWidgetsState();

    Filename suggestedFileName(const Filename& baseName) const;

    void renderTabState();

    void renderState();

private:
    class Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::ExportSettingsDialog> ui;
    FileNameValidator isFileNameValid;
    ExportPasswordWidget* m_passwordWidget;
};

} // namespace nx::vms::client::desktop
