#ifndef QN_CAMERA_SCHEDULE_WIDGET_H
#define QN_CAMERA_SCHEDULE_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/misc/schedule_task.h>

class QnWorkbenchContext;

namespace Ui {
    class CameraScheduleWidget;
}

class QnCameraScheduleWidget: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<QnScheduleTask::Data> scheduleTasks READ scheduleTasks WRITE setScheduleTasks NOTIFY scheduleTasksChanged USER true DESIGNABLE false)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

public:
    QnCameraScheduleWidget(QWidget *parent = 0);
    virtual ~QnCameraScheduleWidget();

    virtual bool hasHeightForWidth() const override;

    /**
     * @brief beginUpdate           Begin component update process. Inner update signals are not called while in update.
     */
    void beginUpdate();

    /**
     * @brief endUpdate             End component update process.
     */
    void endUpdate();

    void setChangesDisabled(bool);
    bool isChangesDisabled() const;

    QList<QnScheduleTask::Data> scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList taskFrom);
    void setScheduleTasks(const QList<QnScheduleTask::Data> &tasks);
    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    /**
     * @brief setMaxFps             Set maximum fps value that can be placed on the grid.
     * @param value                 Maximum allowed fps value for record types "always" and "motion only".
     * @param dualStreamValue       Maximum allowed fps value for record type "motion-plus-lq".
     */
    void setMaxFps(int value, int dualStreamValue);
    void setFps(int value);

    /**
     * @brief getGridMaxFps         Get maximum fps value placed on a grid. If parameter motionPlusLqOnly set, then
     *                              only cells with recording type RecordingType_MotionPlusLQ will be taken into account.
     * @param motionPlusLqOnly      Whether we should check only cells with recording type RecordingType_MotionPlusLQ.
     * @return                      Maximum fps value.
     */
    int getGridMaxFps(bool motionPlusLqOnly = false);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMotionAvailable(bool available);
    bool isMotionAvailable() const;

    void setRecordingParamsAvailability(bool available);
    bool isRecordingParamsAvailable() const;

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    /** Returns true if there is at least one "record-motion" square on the grid */
    bool hasMotionOnGrid() const;

    /** Returns true if there is at least one "record-motion-plus-LQ-always" square on the grid */
    bool hasDualStreamingMotionOnGrid() const;

    // TODO
    QnWorkbenchContext *context() const { return m_context; }
    void setContext(QnWorkbenchContext *context);

    void setExportScheduleButtonEnabled(bool enabled);
    int maxRecordedDays() const;
    int minRecordedDays() const;
    static const int RecordedDaysDontChange = INT_MAX;
signals:
    void archiveRangeChanged();
    void scheduleTasksChanged();
    void recordingSettingsChanged();
    void scheduleEnabledChanged(int);
    void moreLicensesRequested();
    void gridParamsChanged();
    void scheduleExported(const QnVirtualCameraResourceList &);
    void controlsChangesApplied();

private slots:
    void updateGridParams(bool fromUserInput = false);
    void updateGridEnabledState();
    void updateArchiveRangeEnabledState();
    void validateArchiveLength();
    void updateLicensesLabelText();
    void updateMotionButtons();
    void updatePanicLabelText();
    void updateLicensesButtonVisible();
    void updateRecordSpinboxes();
    void updateMaxFpsValue(bool motionPlusLqToggled);

    void at_gridWidget_cellActivated(const QPoint &cell);
    void at_enableRecordingCheckBox_clicked();
    void at_checkBoxArchive_clicked();
    void at_displayQualiteCheckBox_stateChanged(int state);
    void at_displayFpsCheckBox_stateChanged(int state);
    void at_licensesButton_clicked();
    void at_releaseSignalizer_activated(QObject *target);
    void at_exportScheduleButton_clicked();
private:
    int qualityToComboIndex(const Qn::StreamQuality& q);

    void connectToGridWidget();
    void disconnectFromGridWidget();
private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;
    QnWorkbenchContext *m_context;

    QnVirtualCameraResourceList m_cameras;
    bool m_disableUpdateGridParams;
    bool m_motionAvailable;
    bool m_recordingParamsAvailable;
    bool m_changesDisabled;
    bool m_readOnly;

    /**
     * @brief m_maxFps                  Maximum fps value for record types "always" and "motion only"
     */
    int m_maxFps;

    /**
     * @brief m_maxDualStreamingFps     Maximum fps value for record types "motion-plus-lq"
     */
    int m_maxDualStreamingFps;

    /**
     * @brief m_inUpdate                Counter that will prevent unnessesary calls when component update is in progress.
     */
    int m_inUpdate;
};


#endif // QN_CAMERA_SCHEDULE_WIDGET_H
