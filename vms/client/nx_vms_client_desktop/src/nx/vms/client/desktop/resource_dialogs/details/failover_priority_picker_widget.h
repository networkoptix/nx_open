// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include <nx/vms/api/types/resource_types.h>

namespace Ui { class FailoverPriorityPickerWidget; }

namespace nx::vms::client::desktop {

class FailoverPriorityPickerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    FailoverPriorityPickerWidget(QWidget* parent = nullptr);
    virtual ~FailoverPriorityPickerWidget() override;

    void switchToPicker();
    void switchToPlaceholder();

signals:
    void failoverPriorityClicked(nx::vms::api::FailoverPriority failoverPriority);

private:
    const std::unique_ptr<Ui::FailoverPriorityPickerWidget> ui;
};

} // namespace nx::vms::client::desktop
