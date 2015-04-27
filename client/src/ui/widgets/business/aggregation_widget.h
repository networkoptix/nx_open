#ifndef AGGREGATION_WIDGET_H
#define AGGREGATION_WIDGET_H

#include <QWidget>

namespace Ui {
    class AggregationWidget;
}

class QnAggregationWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnAggregationWidget(QWidget *parent = 0);
    ~QnAggregationWidget();
    
    Q_SLOT void setValue(int secs);
    int value() const;

    void setShort(bool value);

    static int optimalWidth();
signals:
    void valueChanged();

private:
    QScopedPointer<Ui::AggregationWidget> ui;
};

#endif // AGGREGATION_WIDGET_H
