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
    Q_PROPERTY(QList<QnScheduleTask::Data> scheduleTasks READ scheduleTasks WRITE setScheduleTasks USER true DESIGNABLE false)

public:
    QnCameraScheduleWidget(QWidget *parent = 0);

    void setDoNotChange(bool);
    bool isDoNotChange() const;

    QList<QnScheduleTask::Data> scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList taskFrom);
    void setScheduleTasks(const QList<QnScheduleTask::Data> &tasks);
    void setScheduleDisabled(Qt::CheckState checkState);
    void setMaxFps(int value);

    Qt::CheckState getScheduleDisabled() const;

private Q_SLOTS:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void onDisableScheduleClicked();
    void updateGridParams();
    void onNeedReadCellParams(const QPoint &cell);

private:
    int qualityTextToIndex(const QString &text);

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    Ui::CameraSchedule *const ui;
    bool m_disableUpdateGridParams;
};


#endif // __CAMERA_SCHEDULE_H_
