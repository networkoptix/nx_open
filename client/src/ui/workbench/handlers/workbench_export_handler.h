#ifndef WORKBENCH_EXPORT_HANDLER_H
#define WORKBENCH_EXPORT_HANDLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnTimePeriod;

/**
 * @brief The QnWorkbenchExportHandler class            Handler for video and layout export related actions.
 */
class QnWorkbenchExportHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchExportHandler(QObject *parent = NULL);

    bool saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable, QObject *target = NULL, const char *slot = NULL);

    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, const QnLayoutResourcePtr &layout, Qn::LayoutExportMode mode);
private:
#ifdef Q_OS_WIN
    QString binaryFilterName() const;
#endif

    bool validateItemTypes(const QnLayoutResourcePtr &layout); // used for export local layouts. Disable cameras and local items for same layout
    void removeLayoutFromPool(const QnLayoutResourcePtr &existingLayout);

    bool saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
                               const QnTimePeriod& exportPeriod,
                               const QString& layoutFileName,
                               Qn::LayoutExportMode mode,
                               bool readOnly,
                               bool cancellable,
                               QObject *target = NULL,
                               const char *slot = NULL);

    bool lockFile(const QString &filename);
    void unlockFile(const QString &filename);

private slots:
    void at_exportTimeSelectionAction_triggered();

    void at_exportLayoutAction_triggered();
    void at_layout_exportFinished(bool success, const QString &filename);
    void at_camera_exportFinished(bool success, const QString &fileName);

private:
    QSet<QString> m_filesIsUse;
};

#endif // WORKBENCH_EXPORT_HANDLER_H
