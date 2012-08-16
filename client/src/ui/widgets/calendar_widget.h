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
public: 
    QnCalendarWidget();
    void setCurrentTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods);
    void setSyncedTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods);
    void setSelectedWindow(quint64 windowStart, quint64 windowEnd);

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
    void dateUpdate(const QDate &date); // TODO: signals that are part of public interface are named as 'somethingHappened', in this case - 'dateClicked'

protected:
    virtual void paintCell(QPainter * painter, const QRect & rect, const QDate & date ) const override;

protected slots:
    // TODO: slots are named either as 'doSomething' or as 'at_somewhere_somethingHappened', where 'somewhere' is a name of an object,
    // and 'somethingHappened' is a name of a signal of this object that this slot handles. In this case a better name is 'at_tableView_changeDate'
    // 
    // OR even better, just connect tableView's signal directly to 'dateClicked' signal of this object.

    void dateChanged(const QDate &date); 

private:
    QnTimePeriodStorage m_currentTimeStorage;
    QnTimePeriodStorage m_syncedTimeStorage;
    QTableView* m_tableView;
    QnTimePeriod m_window;
};


#endif // QN_CALENDAR_WIDGET_H
