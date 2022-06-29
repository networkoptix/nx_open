// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/export/data/export_types.h>


namespace nx::vms::client::desktop {

class AbstractExportTool: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    AbstractExportTool(QObject* parent = nullptr);
    virtual ~AbstractExportTool() override;

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual ExportProcessError lastError() const = 0;
    virtual ExportProcessStatus processStatus() const = 0;

signals:
    void rangeChanged(int from, int to);
    void valueChanged(int value);
    void statusChanged(ExportProcessStatus status);
    void finished();
};

} // namespace nx::vms::client::desktop
