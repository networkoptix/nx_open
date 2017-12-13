#ifndef TIME_DURATION_WIDGET_H
#define TIME_DURATION_WIDGET_H

#include <QtWidgets/QWidget>

#include <text/time_strings.h>

namespace Ui {
    class TimeDurationWidget;
}

class QnTimeDurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnTimeDurationWidget(QWidget *parent = 0);
    ~QnTimeDurationWidget();

    // Add duration usffix to dropdown box
    // This should be called before setValue
    void addDurationSuffix(QnTimeStrings::Suffix suffix);
    Q_SLOT void setValue(int secs);
    int value() const;

    static int optimalWidth();

    void setEnabled(bool value);

    void setMaximum(int value);

    QWidget *lastTabItem();

signals:
    void valueChanged();

private:
    void updateMinimumValue();

private:
    QScopedPointer<Ui::TimeDurationWidget> ui;
};

#endif // AGGREGATION_WIDGET_H
