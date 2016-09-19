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

    qint64 startTimeMs() const;
    qint64 endTimeMs() const;
    void setRange(qint64 startTimeMs, qint64 endTimeMs);

signals:
    void rangeChanged(qint64 startTimeMs, qint64 endTimeMs);

private:
    void updateRange();
    QDateTime actualDateTime(const QDate &userDate) const;
    QDate displayDate(qint64 timestampMs) const;

private:
    QScopedPointer<Ui::DateRangeWidget> ui;
    qint64 m_startTimeMs;
    qint64 m_endTimeMs;
};
