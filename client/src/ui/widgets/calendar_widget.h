#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H

#include <QtGui/QCalendarWidget>
#include <QtGui/QTableView>

#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <recording/time_period_storage.h>

class QnCalendarItemDelegate;

/**
 * Widget for displaying calendar supporting cell highlighting
 * based on the time period lists
 */
class QnCalendarWidget: public QCalendarWidget {
    Q_OBJECT
    typedef QCalendarWidget base_type;

public: 
    QnCalendarWidget(QWidget *parent = NULL);
    
    void setCurrentTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);
    void setSyncedTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);
    void setSelectedWindow(quint64 windowStart, quint64 windowEnd);

    // TODO: #GDM please do something about this setter. The naming is terribly wrong
    // even in the context of our application (see call site in workbench_navigator).
  
    // TODO: #GDM function/accessor name should convey what it does/modifies,
    // not where it should be used in some other class. 
    // 
    // Calendar knows nothing about central / non-central widgets, resources,
    // and other our stuff, and it really shouldn't. If it is used outside of
    // the context of our application, its method names should still make sense.
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

    virtual void paintCell(QPainter * painter, const QRect & rect, const QDate & date ) const override;

private:
    Q_SLOT void updateEmpty();
    Q_SLOT void updateEnabledPeriod();
    Q_SLOT void updateCurrentTime();

private:
    QnCalendarItemDelegate *m_delegate;
    QnTimePeriodStorage m_currentPeriodStorage;
    QnTimePeriodStorage m_syncedPeriodStorage;
    QnTimePeriodStorage m_emptyPeriodStorage;
    QnTimePeriod m_selectedPeriod, m_selectedDaysPeriod, m_enabledPeriod;
    bool m_empty;
    bool m_currentWidgetIsCentral;

    /** Current server time in msecs since epoch. */
    qint64 m_currentTime;
};


#endif // QN_CALENDAR_WIDGET_H
