// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class ExportSettingsDialog;
struct ExportProcessInfo;
class AbstractExportTool;
class ExportSettingsDialog;

/**
 * @brief Handler for video and layout export related actions.
 */
class WorkbenchExportHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;
public:
    WorkbenchExportHandler(QObject* parent = nullptr);
    virtual ~WorkbenchExportHandler() override;

private:
    using Settings = std::variant<ExportMediaSettings, ExportLayoutSettings>;

    void at_exportStandaloneClientAction_triggered();
    void at_saveLocalLayoutAction_triggered();

    void exportProcessUpdated(const ExportProcessInfo& info);
    void exportProcessFinished(const ExportProcessInfo& info);

    void handleExportVideoAction(const menu::Parameters& parameters);
    void handleExportBookmarkAction(const menu::Parameters& parameters);

    void handleExportBookmarksAction();
    typedef std::pair<QnUuid, std::unique_ptr<AbstractExportTool>> ExportToolInstance;
    // Extracts selected parameters from the dialog and prepares appropriate export tool.
    ExportToolInstance prepareExportTool(const ExportSettingsDialog& dialog);
    ExportToolInstance prepareExportTool(
        Settings settings,
        const QnResourcePtr& resource,
        bool saveExistingLayout, bool forceTranscoding);

    void runExport(ExportToolInstance&& context);

    bool validateItemTypes(const QnLayoutResourcePtr& layout) const;
private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
