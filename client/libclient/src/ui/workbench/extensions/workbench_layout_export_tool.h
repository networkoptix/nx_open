#ifndef WORKBENCH_LAYOUT_EXPORT_TOOL_H
#define WORKBENCH_LAYOUT_EXPORT_TOOL_H

#include <QtCore/QObject>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>
#include <recording/stream_recorder.h>

#include <ui/workbench/workbench_context_aware.h>

class QnClientVideoCamera;

/**
 * @brief The QnLayoutExportTool class          Utility class for exporting multi-video layouts.
 *                                              New instance of the class should be created for every new export process.
 *                                              Correct behaviour while re-using is not guarantied.
 *                                              Notifies about progress of the process.
 */
class QnLayoutExportTool : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    /**
     * @brief QnLayoutExportTool                Constructor method.
     * @param layout                            Layout that should be exported.
     * @param period                            Time period to export.
     * @param filename                          Target filename.
     * @param mode                              Mode of exporting: save over existing file,
     *                                          save it to other location or add new exported layout.
     * @param readOnly                          Read mode for new layout: full access or read-only.
     * @param parent                            Owner object.
     */
    explicit QnLayoutExportTool(const QnLayoutResourcePtr &layout,
                                const QnTimePeriod& period,
                                const QString& filename,
                                Qn::LayoutExportMode mode,
                                bool readOnly,
                                QObject *parent = 0);

    /**
     * @brief start                             Start exporting. Async.
     * @return                                  True if there is at least one camera to export on the layout.
     */
    bool start();

    /**
     * @brief stop                              Stop exporting. Async.
     */
    Q_SLOT void stop();

    /**
     * @brief mode                              Current export mode.
     * @return
     */
    Qn::LayoutExportMode mode() const;

    /**
     * @brief errorMessage                      Last error message. Filled if process finished incorrectly.
     * @return
     */
    QString errorMessage() const;
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
     * @brief stageChanged                      This signal is emitted when process stage is changed (e.g. new camera started to export).
     * @param title                             Translatable string with the status of the process.
     */
    void stageChanged(QString title);

    /**
     * @brief finished                          This signal is emitted when the process is finished.
     * @param success                           True if the process is finished successfully, false otherwise.
     */
    void finished(bool success, const QString &filename);
private slots:
    bool exportMediaResource(const QnMediaResourcePtr& resource);

    void at_camera_progressChanged(int progress);
    void at_camera_exportFinished(const StreamRecorderErrorStruct& status,
        const QString& filename);

    void at_camera_exportStopped();

private:
    /** Info about layout items */
    struct ItemInfo {
        QString name;
        qint64 timezone;

        ItemInfo();
        ItemInfo(const QString name, qint64 timezone);
    };
    typedef QList<ItemInfo> ItemInfoList;

    /** Create and setup storage resource. */
    bool prepareStorage();

    /** Create and setup layout resource. */
    ItemInfoList prepareLayout();

    /** Write metadata to storage file. */
    bool exportMetadata(const ItemInfoList &items);

    bool exportNextCamera();
    void finishExport(bool success);

    bool tryAsync(std::function<bool()> handler);
    bool writeData(const QString &fileName, const QByteArray &data);
private:
    /** Copy of the provided layout. */
    QnLayoutResourcePtr m_layout;

    /** List of resources to export. */
    QQueue<QnMediaResourcePtr> m_resources;

    /** External file storage. */
    QnStorageResourcePtr m_storage;

    /** Exported time period. */
    QnTimePeriod m_period;

    /** Name of the file the layout should be exported to. */
    QString m_targetFilename;

    /** Name of the file we are writing. May differ from target filename if target file is busy. */
    QString m_realFilename;

    /** Current export mode. */
    Qn::LayoutExportMode m_mode;

    /** Access status of the target layout. */
    bool m_readOnly;

    /** Stage field, used to show correct progress in multi-video layout. */
    int m_offset;

    /** Last error message. */
    QString m_errorMessage;

    /** Additional flag, set to true when the process is stopped from outside. */
    bool m_stopped;

    QnClientVideoCamera *m_currentCamera;
};

#endif // WORKBENCH_LAYOUT_EXPORT_TOOL_H
