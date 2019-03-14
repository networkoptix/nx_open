#pragma once

#include <QtWidgets/QTabWidget>

namespace nx::vms::client::desktop {

/**
 * Replacement for standard tab widget that allows passing custom tab bar to a constructor.
 */
class TabWidget: public QTabWidget
{
    Q_OBJECT
    using base_type = QTabWidget;

public:
    explicit TabWidget(QWidget* parent = nullptr);
    explicit TabWidget(QTabBar* tabBar, QWidget* parent = nullptr); //< Tab bar ownership is taken.
};

} // namespace nx::vms::client::desktop
