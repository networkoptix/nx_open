#ifndef QN_WEEKTIME_SCHEDULE_WIDGET_H
#define QN_WEEKTIME_SCHEDULE_WIDGET_H

#include <QtGui/QWidget>
#include <QtGui/QDialog>

class QnWorkbenchContext;

namespace Ui {
    class WeekTimeScheduleWidget;
}

class QnWeekTimeScheduleWidget: public QDialog
{
    Q_OBJECT
public:
    QnWeekTimeScheduleWidget(QWidget *parent = 0);
    virtual ~QnWeekTimeScheduleWidget();

    /**
     * @Return binary data witch schedule. Each hour in a week represented as single bit. Data is converted to HEX string
     */
    QString scheduleTasks() const;
    void setScheduleTasks(const QString& schedule);
signals:
    void scheduleTasksChanged();
    void gridParamsChanged();

private slots:
    void updateGridParams(bool fromUserInput = false);
    
    void at_gridWidget_cellActivated(const QPoint &cell);
private:
    void connectToGridWidget();
    void disconnectFromGridWidget();

private:
    Q_DISABLE_COPY(QnWeekTimeScheduleWidget)

    QScopedPointer<Ui::WeekTimeScheduleWidget> ui;
    QnWorkbenchContext *m_context;

    bool m_disableUpdateGridParams;

    /**
     * @brief m_inUpdate                Counter that will prevent unnessesary calls when component update is in progress.
     */
    int m_inUpdate;
};


#endif // QN_WEEKTIME_SCHEDULE_WIDGET_H
