#pragma once

#include <text/tr_functions.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace ptz {

QN_DECLARE_TR_FUNCTIONS("nx::client::desktop::ui::ptz")

bool deletePresetInUse(QWidget* parent);

void failedToGetPosition(QWidget* parent, const QString& cameraName);

void failedToSetPosition(QWidget* parent, const QString& cameraName);

} // namespace ptz
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
