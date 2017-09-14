#pragma once

#include <QtCore/QObject>

#include <nx/client/desktop/export/data/export_types.h>

#include <recording/stream_recorder.h>

#include <utils/common/connective.h>

class QnClientVideoCamera;

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;

class ExportMediaTool : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit ExportMediaTool(const ExportMediaSettings& settings, QObject* parent = nullptr);
    virtual ~ExportMediaTool();

    /**
     * Start export.
     */
    bool start();

    /**
     * Export status.
     */
    StreamRecorderError status() const;

    ExportProcessStatus processStatus() const;

    /**
     * Stop exporting.
     */
    void stop();

signals:
    void rangeChanged(int from, int to);
    void valueChanged(int value);
    void statusChanged(ExportProcessStatus status);
    void finished();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
