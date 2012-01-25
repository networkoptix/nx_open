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
    Q_PROPERTY(QnScheduleTaskList scheduleTasks READ scheduleTasks WRITE setScheduleTasks USER true DESIGNABLE false)

public:
    CameraScheduleWidget(QWidget *parent = 0);

    QnScheduleTaskList scheduleTasks() const;
    void setScheduleTasks(const QnScheduleTaskList &tasks);

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
