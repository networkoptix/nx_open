#ifndef WORKBENCH_LAYOUT_EXPORT_TOOL_H
#define WORKBENCH_LAYOUT_EXPORT_TOOL_H

#include <QtCore/QObject>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>

#include <ui/workbench/workbench_context_aware.h>

class QnLayoutExportTool : public QObject, protected QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnLayoutExportTool(const QnLayoutResourcePtr &layout,
                                const QnTimePeriod& period,
                                const QString& filename,
                                Qn::LayoutExportMode mode,
                                bool readOnly,
                                QObject *parent = 0);

    bool start();

    Q_SLOT void stop();

    Qn::LayoutExportMode mode() const;
    QString errorMessage() const;
signals:
    void rangeChanged(int from, int to);
    void valueChanged(int value);
    void stageChanged(QString title);

    void stopped();
    void finished(bool success);
private slots:
    bool exportNextCamera();
    bool exportMediaResource(const QnMediaResourcePtr& resource);
    void finishCameraExport();

    void at_camera_progressChanged(int progress);
    void at_camera_exportFailed(QString errorMessage);
private:
    void finishExport(bool success);

private:
    QnLayoutResourcePtr m_layout;
    QQueue<QnMediaResourcePtr> m_resources;
    QnStorageResourcePtr m_storage;
    QnTimePeriod m_period;
    QString m_targetFilename;
    QString m_realFilename;
    Qn::LayoutExportMode m_mode;
    bool m_readOnly;

    int m_offset;
    QString m_errorMessage;
};

#endif // WORKBENCH_LAYOUT_EXPORT_TOOL_H
