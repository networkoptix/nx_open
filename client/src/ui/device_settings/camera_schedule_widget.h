#ifndef __CAMERA_SCHEDULE_H_
#define __CAMERA_SCHEDULE_H_

#include <QtGui/QWidget>

#include <core/misc/scheduleTask.h>

namespace Ui {
    class CameraSchedule;
}

class CameraScheduleWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<QnScheduleTask::Data> scheduleTasks READ scheduleTasks WRITE setScheduleTasks USER true DESIGNABLE false)

public:
    CameraScheduleWidget(QWidget *parent = 0);

    QList<QnScheduleTask::Data> scheduleTasks() const;
    void setScheduleTasks(const QList<QnScheduleTask::Data> &tasks);

    void setMaxFps(int value);
private Q_SLOTS:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void updateGridParams();
    void onNeedReadCellParams(const QPoint &cell);

private:
    int qualityTextToIndex(const QString &text);

private:
    Q_DISABLE_COPY(CameraScheduleWidget)

    Ui::CameraSchedule *const ui;
    bool m_disableUpdateGridParams;
};


#endif // __CAMERA_SCHEDULE_H_
