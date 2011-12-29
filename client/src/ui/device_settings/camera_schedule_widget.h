#ifndef __CAMERA_SCHEDULE_H_
#define __CAMERA_SCHEDULE_H_

#include <QWidget>

namespace Ui {
    class CameraSchedule;
};


class CameraScheduleWidget : public QWidget
{
    Q_OBJECT
public:
    CameraScheduleWidget(QWidget *parent);
private:
    Ui::CameraSchedule *ui;
};


#endif // __CAMERA_SCHEDULE_H_
