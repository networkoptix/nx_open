#ifndef WEEK_TIME_SCHEDULE_DIALOG_H
#define WEEK_TIME_SCHEDULE_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
    class WeekTimeScheduleDialog;
}

class QnWeekTimeScheduleDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnWeekTimeScheduleDialog(QWidget *parent = 0);
    ~QnWeekTimeScheduleDialog();

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
    Q_DISABLE_COPY(QnWeekTimeScheduleDialog)

    QScopedPointer<Ui::WeekTimeScheduleDialog> ui;

    bool m_disableUpdateGridParams;

    /**
     * @brief m_inUpdate                Counter that will prevent unnessesary calls when component update is in progress.
     */
    int m_inUpdate;
};

#endif // WEEK_TIME_SCHEDULE_DIALOG_H
