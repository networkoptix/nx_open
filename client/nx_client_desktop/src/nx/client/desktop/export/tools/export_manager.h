#pragma once

#include <QtCore/QObject>

#include <nx/client/desktop/export/data/export_types.h>
#include <utils/common/connective.h>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportProcessInfo
{
    explicit ExportProcessInfo(const QnUuid& id):
        id(id)
    {
    }

    const QnUuid id;
    int rangeStart = 0;
    int rangeEnd = 100;
    int progressValue = 0;
    ExportProcessStatus status = ExportProcessStatus::initial;
    ExportProcessError error = ExportProcessError::noError;
};

class ExportProcess: public Connective<QObject>
{
    using base_type = Connective<QObject>;
    Q_OBJECT

public:
    ExportProcess(const QnUuid& id, AbstractExportTool* tool, QObject* parent = nullptr);
    virtual ~ExportProcess() override;

    const ExportProcessInfo& info() const;

    virtual void start();
    virtual void stop();

    static QString errorString(ExportProcessError error);

signals:
    void infoChanged(const ExportProcessInfo& info);
    void finished(const QnUuid& id);

protected:
    ExportProcessInfo m_info;
    QScopedPointer<AbstractExportTool> const m_tool;
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

    QnUuid exportMedia(const QnUuid& id, const ExportMediaSettings& settings);
    QnUuid exportLayout(const QnUuid& id, const ExportLayoutSettings& settings);
    void stopExport(const QnUuid& exportProcessId);

    ExportProcessInfo info(const QnUuid& exportProcessId) const;

signals:
    void processUpdated(const ExportProcessInfo& info);
    void processFinished(const ExportProcessInfo& info);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
