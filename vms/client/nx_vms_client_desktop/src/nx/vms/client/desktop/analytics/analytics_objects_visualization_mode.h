#pragma once

namespace nx::vms::client::desktop {

enum class AnalyticsObjectsVisualizationMode
{
    /** Always display analytics objects for the selected camera. */
    always,

    /** Display objects only when "Objects" tab is selected in the right panel. */
    tabOnly,

    /** Undefined mode (used when several cameras with different mode were selected). */
    undefined,
};

} // namespace nx::vms::client::desktop
