#ifndef QN_CAMERA_SCHEDULE_WIDGET_H
#define QN_CAMERA_SCHEDULE_WIDGET_H

#include <QtGui/QWidget>

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

    void setChangesDisabled(bool);
    bool isChangesDisabled() const;

    QList<QnScheduleTask::Data> scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList taskFrom);
    void setScheduleTasks(const QList<QnScheduleTask::Data> &tasks);
    void setScheduleEnabled(bool enabled);
    void setMaxFps(int value);
    int getMaxFps() const;
    void setFps(int value);

    int activeCameraCount() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void setMotionAvailable(bool available);
    bool isMotionAvailable();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);
    int getGridMaxFps();
    bool isSecondaryStreamReserver() const;

    /** Returns true if there is at least one "record-motion" square on the grid */
    bool hasMotionOnGrid() const;

    // TODO
    QnWorkbenchContext *context() const { return m_context; }
    void setContext(QnWorkbenchContext *context);

    /** Set if widget has unsaved changes. */
    void setHasChanges(bool hasChanges);

signals:
    void scheduleTasksChanged();
    void recordingSettingsChanged();
    void scheduleEnabledChanged();
    void moreLicensesRequested();
    void gridParamsChanged();
    void scheduleExported(const QnVirtualCameraResourceList &);

private slots:
    void updateGridParams(bool fromUserInput = false);
    void updateGridEnabledState();
    void updateLicensesLabelText();
    void updateMotionButtons();
    void updatePanicLabelText();
    void updateLicensesButtonVisible();
    void updateRecordSpinboxes();

    void at_gridWidget_cellActivated(const QPoint &cell);
    void at_enableRecordingCheckBox_clicked();
    void at_displayQualiteCheckBox_stateChanged(int state);
    void at_displayFpsCheckBox_stateChanged(int state);
    void at_licensesButton_clicked();
    void at_releaseSignalizer_activated(QObject *target);
    void at_exportScheduleButton_clicked();

private:
    int qualityTextToIndex(const QString &text);

    void connectToGridWidget();
    void disconnectFromGridWidget();

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;
    QnWorkbenchContext *m_context;

    QnVirtualCameraResourceList m_cameras;
    bool m_disableUpdateGridParams;
    bool m_motionAvailable;
    bool m_changesDisabled;
    bool m_readOnly;

    /** Widget has unsaved changes */
    bool m_hasChanges;
};


#endif // QN_CAMERA_SCHEDULE_WIDGET_H
