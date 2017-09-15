#pragma once

#include <nx/client/desktop/export/tools/abstract_export_tool.h>

#include <recording/stream_recorder.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;

class ExportMediaTool: public AbstractExportTool
{
    Q_OBJECT
    using base_type = AbstractExportTool;

public:
    explicit ExportMediaTool(const ExportMediaSettings& settings, QObject* parent = nullptr);
    virtual ~ExportMediaTool();

    virtual bool start() override;
    virtual void stop() override;

    virtual ExportProcessError lastError() const override;
    virtual ExportProcessStatus processStatus() const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
