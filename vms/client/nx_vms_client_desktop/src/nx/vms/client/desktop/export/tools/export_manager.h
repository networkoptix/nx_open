// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/export/data/export_types.h>

namespace nx::vms::client::desktop {

struct ExportProcessInfo
{
    explicit ExportProcessInfo(const nx::Uuid& id):
        id(id)
    {
    }

    const nx::Uuid id;
    int rangeStart = 0;
    int rangeEnd = 100;
    int progressValue = 0;
    ExportProcessStatus status = ExportProcessStatus::initial;
    ExportProcessError error = ExportProcessError::noError;
};

/**
 * ExportProcess class deals with showing process of video exporting
 */
class ExportProcess: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    ExportProcess(const nx::Uuid& id, std::unique_ptr<AbstractExportTool>&& tool, QObject* parent = nullptr);
    virtual ~ExportProcess() override;

    const ExportProcessInfo& info() const;

    virtual void start();
    virtual void stop();

    static QString errorString(ExportProcessError error);

signals:
    void infoChanged(const ExportProcessInfo& info);
    void finished(const nx::Uuid& id);

protected:
    ExportProcessInfo m_info;
    std::unique_ptr<AbstractExportTool> m_tool;
};

/**
 * Class for managing ongoing video export processes. Notifies about export status changes.
 */
class ExportManager: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    ExportManager(QObject* parent = nullptr);

    virtual ~ExportManager();

    /**
     * Start export process using specified tool.
     * Manager will take care of removing the tool
     */
    nx::Uuid startExport(const nx::Uuid& id, std::unique_ptr<AbstractExportTool>&& tool);
    void stopExport(const nx::Uuid& exportProcessId);

    ExportProcessInfo info(const nx::Uuid& exportProcessId) const;

signals:
    void processUpdated(const ExportProcessInfo& info);
    void processFinished(const ExportProcessInfo& info);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
