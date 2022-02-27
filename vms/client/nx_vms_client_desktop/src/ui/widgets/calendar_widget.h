// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CALENDAR_WIDGET_H
#define QN_CALENDAR_WIDGET_H

#include <QtWidgets/QCalendarWidget>
#include <QtWidgets/QTableView>

#include <ui/utils/widget_opacity_mixin.h>

#include <recording/time_period.h>
#include <recording/time_period_storage.h>

class QnCalendarItemDelegate;
class QnTimePeriodList;

/**
 * Widget for displaying calendar supporting cell highlighting
 * based on the time period lists
 */
class QnCalendarWidget: public WidgetOpacityMixin<QCalendarWidget> {
    Q_OBJECT
    typedef WidgetOpacityMixin<QCalendarWidget> base_type;

    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged)

public: 
    QnCalendarWidget(QWidget *parent = nullptr);

    int y() const;
    void setY(int y);
    
    void setCurrentTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);
    void setSyncedTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);
    void setSelectedWindow(quint64 windowStart, quint64 windowEnd);

    bool currentTimePeriodsVisible() const;

    /**
     * Set flag that enable or disable drawing of current periods.
     */
    void setCurrentTimePeriodsVisible(bool value);

    /**
     * \return true is all loaded periods of all roles are empty
     */
    bool isEmpty();

    qint64 localOffset() const;
    void setLocalOffset(qint64 utcOffset);

    /**
     * Returns the header height of the calendar
     */
    int headerHeight() const;

signals:
    /**
     * Signal is emitted when empty state is changed, e.g. loaded new time periods.
     */
    void emptyChanged();

    /**
     * Signal is emitted when cell was clicked (even if it is already selected date)
     */
    void dateClicked(const QDate &date);

    void yChanged(int newY);
    void geometryChanged();

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
    QnTimePeriod m_selectedPeriod, m_enabledPeriod;
    bool m_empty;
    bool m_currentTimePeriodsVisible;

    /** Current server time in msecs since epoch. */
    qint64 m_currentTime;

    qint64 m_localOffset;

    QWidget *m_navigationBar;
};


#endif // QN_CALENDAR_WIDGET_H
