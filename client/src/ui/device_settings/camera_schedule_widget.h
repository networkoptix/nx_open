#ifndef QN_CAMERA_SCHEDULE_WIDGET_H
#define QN_CAMERA_SCHEDULE_WIDGET_H

#include <QtGui/QWidget>

#include <core/misc/scheduleTask.h>

namespace Ui {
    class CameraScheduleWidget;
}

class QnCameraScheduleWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<QnScheduleTask::Data> scheduleTasks READ scheduleTasks WRITE setScheduleTasks NOTIFY scheduleTasksChanged USER true DESIGNABLE false)

public:
    QnCameraScheduleWidget(QWidget *parent = 0);
    virtual ~QnCameraScheduleWidget();

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
    void scheduleEnabledChanged();

private slots:
    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void onEnableScheduleClicked();
    void updateGridParams();
    void onCellActivated(const QPoint &cell);

private:
    int qualityTextToIndex(const QString &text);
    void enableGrid(bool value);

    void connectToGridWidget();
    void disconnectFromGridWidget();

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;
    bool m_disableUpdateGridParams;
};


#endif // QN_CAMERA_SCHEDULE_WIDGET_H
