#pragma once

#include <text/tr_functions.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace ptz {

QN_DECLARE_TR_FUNCTIONS(ptz)

static bool deletePresetInUse(QWidget* parent);

static void failedToGetPosition(QWidget* parent, const QString& cameraName);

static void failedToSetPosition(QWidget* parent, const QString& cameraName);

} // namespace ptz
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
