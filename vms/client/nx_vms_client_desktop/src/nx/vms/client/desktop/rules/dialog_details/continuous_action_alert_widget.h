// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class ContinuousActionAlertWidget; }

namespace nx::vms::client::desktop::rules {

class ContinuousActionAlertWidget: public QWidget
{
    Q_OBJECT

public:
    explicit ContinuousActionAlertWidget(QWidget* parent = nullptr);
    virtual ~ContinuousActionAlertWidget() override;

private:
    QScopedPointer<Ui::ContinuousActionAlertWidget> ui;
};

} // namespace nx::vms::client::desktop::rules
