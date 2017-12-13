#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class AggregationWidget; }

class QnAggregationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnAggregationWidget(QWidget *parent = 0);
    ~QnAggregationWidget();

    void setValue(int secs);
    int value() const;

    void setShort(bool value);

    static int optimalWidth();

    QWidget *lastTabItem();

signals:
    void valueChanged();

private:
    QScopedPointer<Ui::AggregationWidget> ui;
};

