// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/export/tools/abstract_export_tool.h>

#include <recording/stream_recorder.h>

namespace nx::vms::client::desktop {

struct ExportMediaSettings;

class ExportMediaTool: public AbstractExportTool
{
    Q_OBJECT
    using base_type = AbstractExportTool;

public:
    explicit ExportMediaTool(
        const ExportMediaSettings& settings,
        QnMediaResourcePtr mediaResource,
        QObject* parent = nullptr);

    virtual ~ExportMediaTool();

    virtual bool start() override;
    virtual void stop() override;

    virtual ExportProcessError lastError() const override;
    virtual ExportProcessStatus processStatus() const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
