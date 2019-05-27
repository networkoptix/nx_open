#pragma once

#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class WidgetProfiler
{
public:
    /**
     * Saves human-readable report file with Qt widgets allocation statistics and Qt widgets
     * hierarchy listed. May be used for detecting Qt widgets leaks, e.g. dangling menus.
     * @param filePath Full path to report file that will be created.
     * @param parent Pointer to root widget in generated hierarchy,
     *     if nullptr, all allocated widgets will be listed.
     */
    static void saveWidgetsReport(const QString& filePath, const QWidget* parent = nullptr);

    /**
     * Generates human-readable text with widgets hierarchy listed.
     * @param parent Pointer to root widget in generated hierarchy,
     *     if nullptr, all allocated widgets will be listed.
     */
    static QString getWidgetHierarchy(const QWidget* parent = nullptr);

    /**
     * Generates human-readable text with count of allocated Qt widgets by type.
     */
    static QString getWidgetStatistics();
};

} // nx::vms::client::desktop
