#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>
#include <utils/common/connective.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>

namespace nx::vms::client::desktop {

class ExportSettingsDialog;
struct ExportProcessInfo;
class AbstractExportTool;
class ExportSettingsDialog;

/**
 * @brief Handler for video and layout export related actions.
 */
class WorkbenchExportHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    WorkbenchExportHandler(QObject* parent = nullptr);
    virtual ~WorkbenchExportHandler() override;

private:
    void at_exportStandaloneClientAction_triggered();
    void at_saveLocalLayoutAction_triggered();

    void exportProcessUpdated(const ExportProcessInfo& info);
    void exportProcessFinished(const ExportProcessInfo& info);

    void handleExportVideoAction(const ui::action::Parameters& parameters);
    void handleExportBookmarkAction(const ui::action::Parameters& parameters);

    void handleExportBookmarksAction();
    typedef std::pair<QnUuid, std::unique_ptr<AbstractExportTool>> ExportToolInstance;
    // Extracts selected parameters from the dialog and prepares appropriate export tool.
    ExportToolInstance prepareExportTool(const ExportSettingsDialog& dialog);
    void runExport(ExportToolInstance&& context);

    bool validateItemTypes(const QnLayoutResourcePtr& layout) const;
private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
