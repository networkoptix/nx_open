// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/workbench/workbench_state_manager.h>

namespace Ui { class ExportSettingsDialog; }

class QnMediaResourceWidget;
struct QnTimePeriod;
class QnWorkbenchContext;
struct QnCameraBookmark;
struct QnLayoutItemData;

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
class ExportSettingsDialog: public QnButtonBoxDialog, public QnSessionAwareDelegate
{
    Q_OBJECT
    Q_ENUMS(Mode)
    using base_type = QnButtonBoxDialog;

public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Mode,
        Media,
        Layout
    )

    using FileNameValidator = std::function<bool (const Filename& fileName, bool quiet)>;
    ExportSettingsDialog(const QnTimePeriod& timePeriod,
        const QnCameraBookmark& bookmark,
        FileNameValidator isFileNameValid,
        const nx::core::Watermark& watermark,
        QWidget* parent = nullptr);

    virtual ~ExportSettingsDialog() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    Mode mode() const;

    ExportMediaSettings exportMediaSettings() const;
    ExportLayoutSettings exportLayoutSettings() const;

    QnMediaResourcePtr mediaResource() const;
    QnLayoutResourcePtr layout() const;

    virtual void accept() override;

    // Making this methods private causes pointless code bloat
    void disableTab(Mode mode, const QString& reason);
    void setLayout(const QnLayoutResourcePtr& layout);
    void setMediaParams(const QnMediaResourcePtr& mediaResource, const QnLayoutItemData& itemData);
    void setBookmarks(const QnCameraBookmarkList& bookmarks);

    virtual int exec() override;

private:
    SelectableTextButton* buttonForOverlayType(ExportOverlayType type);
    void setupSettingsButtons();
    void initSettingsWidgets();
    void updateTabWidgetSize();
    void updateAlerts(Mode mode, const QStringList& weakAlerts, const QStringList& severeAlerts);
    static void updateAlertsInternal(QLayout* layout, const QStringList& texts, bool severe);

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

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportSettingsDialog::Mode)
