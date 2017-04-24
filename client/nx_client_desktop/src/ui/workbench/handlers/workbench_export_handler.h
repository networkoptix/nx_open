#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnTimePeriod;
class QnAbstractStreamDataProvider;
class QnMediaResourceWidget;

/**
 * @brief The QnWorkbenchExportHandler class            Handler for video and layout export related actions.
 */
class QnWorkbenchExportHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnWorkbenchExportHandler(QObject *parent = NULL);

    bool saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable, QObject *target = NULL, const char *slot = NULL);

    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, const QnLayoutResourcePtr &layout, Qn::LayoutExportMode mode);

private:
    QString binaryFilterName() const;

    bool validateItemTypes(const QnLayoutResourcePtr &layout); // used for export local layouts. Disable cameras and local items for same layout

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

    QnMediaResourceWidget* extractMediaWidget(
        const nx::client::desktop::ui::action::Parameters& parameters);

    void exportTimeSelection(const nx::client::desktop::ui::action::Parameters& parameters,
        qint64 timelapseFrameStepMs = 0);

    void exportTimeSelectionInternal(
        const QnMediaResourcePtr &mediaResource,
        const QnAbstractStreamDataProvider *dataProvider,
        const QnLayoutItemData &itemData,
        const QnTimePeriod &period,
        qint64 timelapseFrameStepMs = 0
    );

    /** Check if exe file will be greater than 4 Gb. */
    bool exeFileIsTooBig(const QnLayoutResourcePtr& layout, const QnTimePeriod& period) const;

    /** Check if exe file will be greater than 4 Gb. */
    bool exeFileIsTooBig(const QnMediaResourcePtr& mediaResource, const QnTimePeriod& period) const;

    private slots:
    void at_exportTimeSelectionAction_triggered();
    void at_exportLayoutAction_triggered();
    void at_exportTimelapseAction_triggered();

    void at_layout_exportFinished(bool success, const QString &filename);
    void at_camera_exportFinished(bool success, const QString &fileName);

    void showExportCompleteMessage();

    bool confirmExport(
        QnMessageBoxIcon icon,
        const QString& text,
        const QString& extras) const;

    bool confirmExportTooBigExeFile() const;

private:
    QSet<QString> m_filesIsUse;
};
