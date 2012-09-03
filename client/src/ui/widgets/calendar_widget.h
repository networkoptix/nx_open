#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H

#include <QtGui/QCalendarWidget>
#include <QtGui/QTableView>

#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <recording/time_period_storage.h>

/**
 * Widget for displaying calendar supporting cell highlighting
 * based on the time period lists
 */
class QnCalendarWidget: public QCalendarWidget {
    Q_OBJECT

    typedef QCalendarWidget base_type;

public: 
    QnCalendarWidget();
    void setCurrentTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods);
    void setSyncedTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods);
    void setSelectedWindow(quint64 windowStart, quint64 windowEnd);

    /**
     * If current widget is not central, calendar should not draw current periods.
     */
    void setCurrentWidgetIsCentral(bool currentWidgetIsCentral);

    /**
     * \return true is all loaded periods of all roles are empty
     */
    bool isEmpty();

signals:
    /**
     * Signal is emitted when empty state is changed, e.g. loaded new time periods.
     */
    void emptyChanged();

    /**
     * Signal is emitted when cell was clicked (even if it is already selected date)
     */
    void dateClicked(const QDate &date);

protected:
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    virtual bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void paintCell(QPainter * painter, const QRect & rect, const QDate & date ) const override;

private:
    void updateEmpty();

    QnTimePeriodStorage m_currentTimeStorage;
    QnTimePeriodStorage m_syncedTimeStorage;
    QTableView* m_tableView;
    QnTimePeriod m_window;
    bool m_empty;
    bool m_currentWidgetIsCentral;

    /** Current server time in msecs since epoch */
    qint64 m_currentTime;
};


#endif // QN_CALENDAR_WIDGET_H
