#pragma once

class QWidget;
struct QnWatermarkSettings;

namespace ui {
namespace dialogs {
namespace watermark_preview {

/** returns true if settings were changed */
bool editSettings(QnWatermarkSettings& settings, QWidget* parent);

} // namespace watermark_preview
} // namespace dialogs
} // namespace ui