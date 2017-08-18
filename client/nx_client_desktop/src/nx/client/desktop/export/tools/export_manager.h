#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;

enum class ExportProcessStatus
{
    initializing,
    exporting,
    success,
    error,
};

struct ExportProcessInfo
{
    const QnUuid id = QnUuid::createUuid();
    int rangeStart = 0;
    int rangeEnd = 100;
    int progressValue = 0;
    ExportProcessStatus status = ExportProcessStatus::initializing;
};

class ExportProcess: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    ExportProcess(QObject* parent = nullptr);

signals:
    void infoChanged(const ExportProcessInfo& info);
    void finished(const QnUuid& id);

protected:
    ExportProcessInfo m_info;
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

    QnUuid exportMedia(const ExportMediaSettings& settings);

signals:
    void processUpdated(const ExportProcessInfo& info);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
