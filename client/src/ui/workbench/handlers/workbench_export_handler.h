#ifndef WORKBENCH_EXPORT_HANDLER_H
#define WORKBENCH_EXPORT_HANDLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <ui/workbench/workbench_context_aware.h>


/**
 * @brief The QnWorkbenchExportHandler class            Handler for video and layout export related actions.
 */
class QnWorkbenchExportHandler: public QObject, protected QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchExportHandler(QObject *parent = NULL);

    bool saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
                               const QnTimePeriod& exportPeriod,
                               const QString& layoutFileName,
                               Qn::LayoutExportMode mode,
                               bool readOnly,
                               bool cancellable,
                               QObject *target = NULL,
                               const char *slot = NULL);

    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, QnLayoutResourcePtr layout, Qn::LayoutExportMode mode);
private:
#ifdef Q_OS_WIN
    QString binaryFilterName() const;
#endif

    bool validateItemTypes(QnLayoutResourcePtr layout); // used for export local layouts. Disable cameras and local items for same layout
    void removeLayoutFromPool(QnLayoutResourcePtr existingLayout);

private slots:
    void at_exportTimeSelectionAction_triggered();

    void at_exportLayoutAction_triggered();
    void at_layout_exportFinished(bool success);

    void at_camera_exportFinished(QString fileName);
    void at_camera_exportFailed(QString errorMessage);

};

#endif // WORKBENCH_EXPORT_HANDLER_H
