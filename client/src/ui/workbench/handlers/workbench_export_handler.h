#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnTimePeriod;
class QnAbstractStreamDataProvider;

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
    QString binaryFilterName() const;

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

    bool isBinaryExportSupported() const;

    QnMediaResourceWidget *extractMediaWidget(const QnActionParameters &parameters);

    void exportTimeSelection(const QnActionParameters& parameters, qint64 timelapseFrameStepMs = 0);

    void exportTimeSelectionInternal(
          const QnMediaResourcePtr &mediaResource,
          const QnAbstractStreamDataProvider *dataProvider,
          const QnLayoutItemData &itemData,
          const QnTimePeriod &period,
          qint64 timelapseFrameStepMs = 0
        );

private slots:
    void at_exportTimeSelectionAction_triggered();
    void at_exportLayoutAction_triggered();
    void at_exportTimelapseAction_triggered();

    void at_layout_exportFinished(bool success, const QString &filename);
    void at_camera_exportFinished(bool success, const QString &fileName);

private:
    QSet<QString> m_filesIsUse;
};
