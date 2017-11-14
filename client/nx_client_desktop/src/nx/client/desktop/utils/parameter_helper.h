#pragma once

class QWidget;

namespace nx {

namespace client {
namespace desktop {
namespace ui {
namespace action {

class Parameters;

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client

namespace utils {

using WidgetPtr = QPointer<QWidget>;

WidgetPtr extractParentWidget(
    const client::desktop::ui::action::Parameters& parameters,
    QWidget* defaultValue);

} // namespace utils

} // namespace nx


