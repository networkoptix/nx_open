// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <limits>

#include <QtWidgets/QWidget>

#include <nx/vms/text/time_strings.h>

class QSpinBox;

namespace Ui { class TimeDurationWidget; }

namespace nx::vms::client::desktop {

class TimeDurationWidget: public QWidget
{
    Q_OBJECT

    typedef QWidget base_type;

public:
    explicit TimeDurationWidget(QWidget *parent = nullptr);
    virtual ~TimeDurationWidget() override;

    // Add duration usffix to dropdown box
    // This should be called before setValue
    void addDurationSuffix(QnTimeStrings::Suffix suffix);

    void setAdditionalText(const QString& additionalText);

    void setValue(int secs);
    int value() const;

    void setMaximum(int secs);
    void setMinimum(int secs);

    QWidget* lastTabItem() const;
    QSpinBox* valueSpinBox() const;

signals:
    void valueChanged();

private:
    void updateMinimumValue();
    void updateMaximumValue();

private:
    QScopedPointer<Ui::TimeDurationWidget> ui;
    QWidget* myWidget = nullptr;
    int m_min = 1;
    int m_max = std::numeric_limits<int>::max();
};

} // namespace nx::vms::client::desktop
