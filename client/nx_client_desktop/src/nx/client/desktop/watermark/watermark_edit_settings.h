#pragma once

class QWidget;
struct QnWatermarkSettings;

namespace nx {
namespace client {
namespace desktop {

/** returns true if settings were changed */
bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent);

} // namespace watermark_preview
} // namespace dialogs
} // namespace ui