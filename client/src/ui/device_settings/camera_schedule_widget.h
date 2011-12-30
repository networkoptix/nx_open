#ifndef __CAMERA_SCHEDULE_H_
#define __CAMERA_SCHEDULE_H_

#include <QtGui/QWidget>

namespace Ui {
    class CameraSchedule;
}

class CameraScheduleWidget : public QWidget
{
    Q_OBJECT

public:
    CameraScheduleWidget(QWidget *parent = 0);

private Q_SLOTS:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void updateGridParams();
    void onNeedReadCellParams(QPoint cell);

private:
    int qualityTextToIndex(const QString& text);

private:
    Q_DISABLE_COPY(CameraScheduleWidget)

    Ui::CameraSchedule *ui;
    bool m_disableUpdateGridParams;
};


#endif // __CAMERA_SCHEDULE_H_
