#pragma once

#include <QtWidgets/QWidget>

#include <core/misc/schedule_task.h>
#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui {
class CameraScheduleWidget;
}

class QnCameraScheduleWidget: public Connective<QWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = Connective<QWidget>;

public:
    explicit QnCameraScheduleWidget(QWidget* parent = 0);
    virtual ~QnCameraScheduleWidget();

    void overrideMotionType(Qn::MotionType motionTypeOverride = Qn::MT_Default);

    QnScheduleTaskList scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList& value);

    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void updateFromResources();
    void submitToResources();

    /** Returns true if there is at least one "record-motion" square on the grid */
    bool hasMotionOnGrid() const;

    /** Returns true if there is at least one "record-motion-plus-LQ-always" square on the grid */
    bool hasDualStreamingMotionOnGrid() const;

    void setExportScheduleButtonEnabled(bool enabled);
    int maxRecordedDays() const;
    int minRecordedDays() const;

    /** Returns true if there's any recording requested in the schedule */
    bool isRecordingScheduled() const;

signals:
    void archiveRangeChanged();
    void scheduleTasksChanged();
    void recordingSettingsChanged();
    void scheduleEnabledChanged(int);
    void gridParamsChanged();
    void controlsChangesApplied();
    void alert(const QString& text);

protected:
    virtual void afterContextInitialized() override;

private:
    bool canEnableRecording() const;

    void updateRecordThresholds(QnScheduleTaskList& tasks);

    void updateScheduleTypeControls();
    void updateGridParams(bool pickedFromGrid = false);
    void updateGridEnabledState();
    void updateArchiveRangeEnabledState();
    void validateArchiveLength();
    void updateLicensesLabelText();
    void updateMotionButtons();
    void updatePanicLabelText();
    void updateLicensesButtonVisible();
    void updateRecordSpinboxes();
    void updateMaxFpsValue(bool motionPlusLqToggled);
    void updateRecordingParamsAvailable();

    void updateColors();

    void at_gridWidget_cellActivated(const QPoint &cell);
    void at_displayQualiteCheckBox_stateChanged(int state);
    void at_displayFpsCheckBox_stateChanged(int state);
    void at_licensesButton_clicked();
    void at_releaseSignalizer_activated(QObject *target);
    void at_exportScheduleButton_clicked();

    void setScheduleAlert(const QString& scheduleAlert);
    void setArchiveLengthAlert(const QString& archiveLengthAlert);

private:
    /**
    * @brief getGridMaxFps         Get maximum fps value placed on a grid. If parameter motionPlusLqOnly set, then
    *                              only cells with recording type RecordingType_MotionPlusLQ will be taken into account.
    * @param motionPlusLqOnly      Whether we should check only cells with recording type RecordingType_MotionPlusLQ.
    * @return                      Maximum fps value.
    */
    int getGridMaxFps(bool motionPlusLqOnly = false);

    void setFps(int value);

    void updateScheduleEnabled();
    void updateMinDays();
    void updateMaxDays();
    void updateMaxFPS();
    void updateMotionAvailable();

    void syncQualityWithBitrate();
    void syncBitrateWithQuality();
    void syncBitrateWithFps();
    void bitrateSpinBoxChanged();
    void bitrateSliderChanged();

    void setAdvancedSettingsVisible(bool value);

    bool isCurrentBitrateCustom() const;
    Qn::StreamQuality currentQualityApproximation() const;

    QPair<Qn::StreamQuality, bool> qualityForBitrate(qreal bitrateMbps) const;
    qreal bitrateForQuality(Qn::StreamQuality quality) const;

    void retranslateUi();

    enum AlertReason
    {
        CurrentParamsChange,
        ScheduleChange,
        EnabledChange
    };

    void updateAlert(AlertReason when);

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;

    QnVirtualCameraResourceList m_cameras;
    bool m_disableUpdateGridParams;
    bool m_motionAvailable;
    bool m_recordingParamsAvailable;
    bool m_readOnly;

    /**
     * @brief m_maxFps                  Maximum fps value for record types "always" and "motion only"
     */
    int m_maxFps;

    /**
     * @brief m_maxDualStreamingFps     Maximum fps value for record types "motion-plus-lq"
     */
    int m_maxDualStreamingFps;

    bool m_advancedSettingsSupported = false;
    bool m_advancedSettingsVisible = false;

    Qn::MotionType m_motionTypeOverride;

    QString m_scheduleAlert;
    QString m_archiveLengthAlert;

    bool m_updating = false;
    bool m_bitrateUpdating = false;
};
