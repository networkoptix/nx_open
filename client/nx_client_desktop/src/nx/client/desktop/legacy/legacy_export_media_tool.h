#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <recording/stream_recorder.h>

#include <utils/common/connective.h>

class QnClientVideoCamera;

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

struct LegacyExportMediaSettings;

class LegacyExportMediaTool : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    LegacyExportMediaTool(
        const LegacyExportMediaSettings& settings,
        QObject* parent = 0);
    virtual ~LegacyExportMediaTool();


    /**
     * @brief start                             Start exporting.
     */
    void start();

    /**
     * @brief status                            Camera exporting status.
     */
    StreamRecorderError status() const;

public slots:
    /**
     * @brief stop                              Stop exporting.
     */
    void stop();

signals:
    /**
     * @brief rangeChanged                      This signal is emitted when progress range of the process is changed.
     * @param from                              Minimum progress value.
     * @param to                                Maximum progress value.
     */
    void rangeChanged(int from, int to);

    /**
     * @brief valueChanged                      This signal is emitted when progress value is changed.
     * @param value                             Current progress value.
     */
    void valueChanged(int value);

    /**
     * @brief finished                          This signal is emitted when the process is finished.
     * @param success                           True if the process is finished successfully, false otherwise.
     */
    void finished(bool success, const QString &filename);

private slots:
    void at_camera_exportFinished(const StreamRecorderErrorStruct& status,
        const QString& filename);

    void at_camera_exportStopped();

private:
    void finishExport(bool success);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
