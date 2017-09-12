#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/export/data/export_layout_settings.h>

#include <utils/common/connective.h>

class QnTimePeriod;
class QnAbstractStreamDataProvider;
class QnMediaResourceWidget;

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

/**
 * @brief Legacy handler for video and layout export related actions.
 */
class WorkbenchExportHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    WorkbenchExportHandler(QObject *parent = NULL);

private:
    bool saveLocalLayout(const QnLayoutResourcePtr& layout,
        bool readOnly,
        bool cancellable);

    bool doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod,
        const QnLayoutResourcePtr& layout,
        ExportLayoutSettings::Mode mode);

    QString binaryFilterName() const;

    bool validateItemTypes(const QnLayoutResourcePtr &layout); // used for export local layouts. Disable cameras and local items for same layout

    bool saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
        const QnTimePeriod& exportPeriod,
        const QString& layoutFileName,
        ExportLayoutSettings::Mode mode,
        bool readOnly,
        bool cancellable);

    bool lockFile(const QString &filename);
    void unlockFile(const QString &filename);

    bool isBinaryExportSupported() const;

    QnMediaResourceWidget* extractMediaWidget(
        const ui::action::Parameters& parameters);

    void exportTimeSelection(const ui::action::Parameters& parameters,
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
    void at_exportRapidReviewAction_triggered();
    void at_saveLocalLayoutAction_triggered();
    void at_saveLocalLayoutAsAction_triggered();

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

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
