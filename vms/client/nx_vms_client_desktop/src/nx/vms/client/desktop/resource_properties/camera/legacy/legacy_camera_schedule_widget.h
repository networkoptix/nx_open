#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui { class LegacyCameraScheduleWidget; }

namespace nx::vms::client::desktop {

struct SchedulePaintFunctions;

class LegacyCameraScheduleWidget: public Connective<QnAbstractPreferencesWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = Connective<QnAbstractPreferencesWidget>;

public:
    // TODO: #GDM Remove snapScrollbarToParent hacky parameter when legacy camera settings gone.
    explicit LegacyCameraScheduleWidget(QWidget* parent = nullptr, bool snapScrollbarToParent = true);
    virtual ~LegacyCameraScheduleWidget() override;

    void setMotionDetectionAllowed(bool value = true);

    QnScheduleTaskList scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList& value);

    void setScheduleEnabled(bool enabled);
    bool isScheduleEnabled() const;

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    /** Returns true if there is at least one "record-motion" square on the grid */
    bool hasMotionOnGrid() const;

    /** Returns true if there is at least one "record-motion-plus-LQ-always" square on the grid */
    bool hasDualStreamingMotionOnGrid() const;

    void setExportScheduleButtonEnabled(bool enabled);

    /** Returns true if there's any recording requested in the schedule */
    bool isRecordingScheduled() const;

signals:
    void scheduleEnabledChanged(int);
    void alert(const QString& text);

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;
    virtual void afterContextInitialized() override;

private:
    bool canEnableRecording() const;

    void updateScheduleTypeControls();
    void updateGridParams(bool pickedFromGrid = false);
    void updateGridEnabledState();
    void updateLicensesLabelText();
    void updateMotionButtons();
    void updateLicensesButtonVisible();
    void updateRecordSpinboxes();
    void updateMaxFpsValue(bool motionPlusLqToggled);
    void updateRecordingParamsAvailable();

    void updateColors();

    void at_cameraResourceChanged();
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

    QPair<int, bool> qualityForBitrate(qreal bitrateMbps) const;
    qreal bitrateForQuality(Qn::StreamQuality quality) const;

    virtual void retranslateUi() override;

    enum AlertReason
    {
        CurrentParamsChange,
        ScheduleChange,
        EnabledChange
    };

    void updateAlert(AlertReason when);

private:
    Q_DISABLE_COPY(LegacyCameraScheduleWidget)

    QScopedPointer<Ui::LegacyCameraScheduleWidget> ui;
    QScopedPointer<SchedulePaintFunctions> paintFunctions;

    QnVirtualCameraResourceList m_cameras;
    bool m_disableUpdateGridParams = false;
    bool m_motionAvailable = true;
    bool m_recordingParamsAvailable = true;

    /**
     * Maximum fps value for record types "always" and "motion only"
     */
    int m_maxFps = 0;

    /**
     * Maximum fps value for record types "motion-plus-lq"
     */
    int m_maxDualStreamingFps = 0;

    bool m_advancedSettingsSupported = false;
    bool m_advancedSettingsVisible = false;

    bool m_motionDetectionAllowed = true;

    QString m_scheduleAlert;

    bool m_updating = false;
    bool m_bitrateUpdating = false;
};

} // namespace nx::vms::client::desktop
