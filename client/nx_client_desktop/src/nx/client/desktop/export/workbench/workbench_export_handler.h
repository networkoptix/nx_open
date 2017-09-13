#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class ExportManager;
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
    void exportProcessUpdated(const ExportProcessInfo& info);

    void handleExportVideoAction();

private:
    QScopedPointer<ExportManager> m_exportManager;
};

} // namespace desktop
} // namespace client
} // namespace nx
