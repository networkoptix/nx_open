// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDate>

#include <QtWidgets/QWidget>

#include <recording/time_period_storage.h>

class QLabel;
class QTableWidgetItem;

class QnDayTimeTableWidget;
class QnDayTimeItemDelegate;

class QnDayTimeWidget: public QWidget {
    Q_OBJECT
    typedef QWidget base_type;

public:
    QnDayTimeWidget(QWidget* parent = nullptr);
    virtual ~QnDayTimeWidget();

    QDate date();
    void setDate(const QDate &date);

    void setPrimaryTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);
    void setSecondaryTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods);

    void setSelectedWindow(quint64 windowStart, quint64 windowEnd);
    void setEnabledWindow(quint64 windowStart, quint64 windowEnd);

    qint64 localOffset() const;
    void setLocalOffset(qint64 utcOffset);

    int headerHeight() const;

signals:
    void timeClicked(const QTime &time);

private:
    friend class QnDayTimeItemDelegate;
    void paintCell(QPainter *painter, const QRect &rect, const QTime &time);

private slots:
    void at_tableWidget_itemClicked(QTableWidgetItem *item);

    void updateEnabled();
    void updateHeaderText();
    void updateCurrentTime();
private:
    QnTimePeriodStorage m_primaryPeriodStorage, m_secondaryPeriodStorage;
    QnTimePeriod m_selectedPeriod, m_selectedHoursPeriod, m_enabledPeriod, m_enabledHoursPeriod;
    QLabel *m_headerLabel;
    QDate m_date;
    qint64 m_currentTime;
    QnDayTimeItemDelegate *m_delegate;
    QnDayTimeTableWidget *m_tableWidget;
    qint64 m_localOffset;
};
