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
private slots:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void updateGridParams();
    void onNeedReadCellParams(QPoint cell);
private:
    int qualityTextToIndex(const QString& text);
private:
    Ui::CameraSchedule *ui;
    bool m_disableUpdateGridParams;
};


#endif // __CAMERA_SCHEDULE_H_
