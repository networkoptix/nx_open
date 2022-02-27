// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

class QWidget;

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

class Parameters;

} // namespace action
} // namespace ui

namespace utils {

using WidgetPtr = QPointer<QWidget>;

WidgetPtr extractParentWidget(
    const ui::action::Parameters& parameters,
    QWidget* defaultValue);

} // namespace utils
} // namespace nx::vms::client::desktop
