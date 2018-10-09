#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class ExportSettingsDialog;
struct ExportProcessInfo;

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
    void exportProcessUpdated(const ExportProcessInfo& info);
    void exportProcessFinished(const ExportProcessInfo& info);

    void handleExportVideoAction();
    void handleExportBookmarkAction();
    void handleExportBookmarksAction();

    void startExportFromDialog(ExportSettingsDialog* dialog);

private:
    struct Private;
    QScopedPointer<Private> d;

    // Figures out if a watermark is needed and sets it to the dialog.
    void setWatermark(nx::client::desktop::ExportSettingsDialog* dialog);
};

} // namespace desktop
} // namespace client
} // namespace nx
