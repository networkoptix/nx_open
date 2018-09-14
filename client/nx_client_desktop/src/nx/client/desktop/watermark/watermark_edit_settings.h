#pragma once

class QWidget;
struct QnWatermarkSettings;

namespace nx {
namespace client {
namespace desktop {

/** Returns true if settings were changed. */
bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent);

} // namespace watermark_preview
} // namespace dialogs
} // namespace ui