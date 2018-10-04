#pragma once

#include <core/resource/resource_fwd.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {
namespace layout {

bool needPassword(const QnLayoutResourcePtr& layout);

bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

} // namespace layout
} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx