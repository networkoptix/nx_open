#pragma once

#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class DateRangeWidget;
}

/**
 *  Class allows to select date range and automatically converts it to UTC, concerning selected
 *  time mode and using server time if needed.
 */
class QnDateRangeWidget: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;
public:
    explicit QnDateRangeWidget(QWidget* parent = nullptr);
    virtual ~QnDateRangeWidget();

    /** Selected UTC date range in milliseconds since epoch. */
    qint64 startTimeMs() const;
    qint64 endTimeMs() const;
    void setRange(qint64 startTimeMs, qint64 endTimeMs);

    /** Selected date range in system time spec. */
    QDate startDate() const;
    QDate endDate() const;

    /** Change values to default dates: since 1/1/2000 to today's date. */
    void reset();

signals:
    void rangeChanged(qint64 startTimeMs, qint64 endTimeMs);

private:
    void updateRange();
    void updateAllowedRange();

    QDateTime actualDateTime(const QDate &userDate) const;
    QDate displayDate(qint64 timestampMs) const;

private:
    QScopedPointer<Ui::DateRangeWidget> ui;
};
