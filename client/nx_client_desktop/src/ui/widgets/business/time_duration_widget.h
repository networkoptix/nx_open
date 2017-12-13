#pragma once

#include <QtWidgets/QWidget>

#include <text/time_strings.h>

namespace Ui { class TimeDurationWidget; }

namespace nx {
namespace client {
namespace desktop {

class TimeDurationWidget : public QWidget
{
    Q_OBJECT

    typedef QWidget base_type;

public:
    explicit TimeDurationWidget(QWidget *parent = 0);
    virtual ~TimeDurationWidget() override;

    // Add duration usffix to dropdown box
    // This should be called before setValue
    void addDurationSuffix(QnTimeStrings::Suffix suffix);

    void setValue(int secs);
    int value() const;

    void setMaximum(int value);

    QWidget *lastTabItem();

signals:
    void valueChanged();

private:
    void updateMinimumValue();

private:
    QScopedPointer<Ui::TimeDurationWidget> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx