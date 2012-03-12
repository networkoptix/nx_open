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
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

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

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    void scheduleTasksChanged();
    void scheduleEnabledChanged();

private slots:
    void updateGridParams(bool fromUserInput = false);
    void updateGridEnabledState();

    void onDisplayQualityChanged(int state);
    void onDisplayFPSChanged(int state);
    void onEnableScheduleClicked();
    void onCellActivated(const QPoint &cell);

private:
    int qualityTextToIndex(const QString &text);

    void connectToGridWidget();
    void disconnectFromGridWidget();

private:
    Q_DISABLE_COPY(QnCameraScheduleWidget)

    QScopedPointer<Ui::CameraScheduleWidget> ui;
    bool m_disableUpdateGridParams;
    bool m_changesDisabled;
    bool m_readOnly;
};


#endif // QN_CAMERA_SCHEDULE_WIDGET_H
