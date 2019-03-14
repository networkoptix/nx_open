#pragma once

class QWidget;
struct QnWatermarkSettings;

namespace nx::vms::client::desktop {

/** Returns true if settings were changed. */
bool editWatermarkSettings(QnWatermarkSettings& settings, QWidget* parent);

} // namespace nx::vms::client::desktop
