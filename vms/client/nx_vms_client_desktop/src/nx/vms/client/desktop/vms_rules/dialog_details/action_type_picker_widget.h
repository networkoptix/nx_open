// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace Ui { class ActionTypePickerWidget; }

namespace nx::vms::client::desktop {
namespace vms_rules {

class ActionTypePickerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    ActionTypePickerWidget(QWidget* parent = nullptr);
    virtual ~ActionTypePickerWidget() override;

private:
    QScopedPointer<Ui::ActionTypePickerWidget> m_ui;
};

} // namespace vms_rules
} // namespace nx::vms::client::desktop
