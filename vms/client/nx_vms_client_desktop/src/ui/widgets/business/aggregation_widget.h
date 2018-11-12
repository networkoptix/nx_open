#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class AggregationWidget; }

namespace Ui { class TimeDurationWidget; }

namespace nx::vms::client::desktop {

class AggregationWidget: public QWidget
{
    Q_OBJECT

    typedef QWidget base_type;

public:
    explicit AggregationWidget(QWidget* parent);
    virtual ~AggregationWidget() override;

    void setValue(int secs);
    int value() const;

    void setShort(bool value);

    static int optimalWidth();

    QWidget* lastTabItem() const;

signals:
    void valueChanged();

private:
    QScopedPointer<Ui::AggregationWidget> ui;
};

} // namespace nx::vms::client::desktop
