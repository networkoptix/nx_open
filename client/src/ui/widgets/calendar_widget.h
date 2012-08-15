#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H

#include <QtGui/QCalendarWidget>

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
    void setCurrentTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods );
    void setSyncedTimePeriods(Qn::TimePeriodRole type, QnTimePeriodList periods );

    /**
    * \return true is all loaded periods of all roles are empty
    */
    bool isEmpty();
signals:
    /**
    * Signal is emitted when empty state is changed, e.g. loaded new time periods.
    */
    void emptyChanged();
protected:
    virtual void paintCell(QPainter * painter, const QRect & rect, const QDate & date ) const override;
private:
    QnTimePeriodStorage m_currentTimeStorage;
    QnTimePeriodStorage m_syncedTimeStorage;
};


#endif // QN_CALENDAR_WIDGET_H
