#ifndef __CAMERA_SCHEDULE_H_
#define __CAMERA_SCHEDULE_H_

#include <QtGui/QWidget>

#include <core/misc/scheduleTask.h>

namespace Ui {
    class CameraSchedule;
}

class QnCameraScheduleWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<QnScheduleTask::Data> scheduleTasks READ scheduleTasks WRITE setScheduleTasks NOTIFY scheduleTasksChanged USER true DESIGNABLE false)

public:
    QnCameraScheduleWidget(QWidget *parent = 0);

    void setChangesDisabled(bool);
    bool isChangesDisabled() const;

    QList<QnScheduleTask::Data> scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList taskFrom);
    void setScheduleTasks(const QList<QnScheduleTask::Data> &tasks);
    void setScheduleEnabled(Qt::CheckState checkState);
    void setMaxFps(int value);

    Qt::CheckState getScheduleEnabled() const;

signals:
    void scheduleTasksChanged();

private slots:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void onEnableScheduleClicked();
    void updateGridParams();
    void onNeedReadCellParams(const QPoint &cell);

private:
    int qualityTextToIndex(const QString &text);
    void enableGrid(bool value);

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraSchedule> ui;
    bool m_disableUpdateGridParams;
};


#endif // __CAMERA_SCHEDULE_H_
