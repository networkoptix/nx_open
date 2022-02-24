// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop::workbench::timeline {

class ClockLabel: public QLabel
{
    Q_OBJECT
    using base_type = QLabel;

public:
    explicit ClockLabel(QWidget* parent = nullptr);
    virtual ~ClockLabel() override;

protected:
    virtual void timerEvent(QTimerEvent*) override;

private:
    int m_timerId = 0;
};

} // nx::vms::client::desktop::workbench::timeline
