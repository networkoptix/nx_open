#pragma once

#include <QtCore/QPointer>

#include <nx/vms/client/desktop/common/widgets/tab_widget.h>

namespace nx::vms::client::desktop {

class TransitionerWidget;

/**
 * A tab widget that makes opacity-animated transition when tabs are switched.
 */
class AnimatedTabWidget: public TabWidget
{
    Q_OBJECT
    using base_type = TabWidget;

public:
    explicit AnimatedTabWidget(QWidget* parent = nullptr);
    explicit AnimatedTabWidget(QTabBar* tabBar, QWidget* parent = nullptr); //< Tab bar ownership is taken.

private:
    void handleCurrentChanged();
    QRect tabRect(QWidget* tab) const; //< Relative to this widget.
    QPixmap grabTab(QWidget* tab, const QSize& size /*logical*/, const QPoint& offset) const;

private:
    QPointer<QWidget> m_currentTab;
    QPointer<QWidget> m_tabsParent;
    TransitionerWidget* const m_transitioner = nullptr;
};

} // namespace nx::vms::client::desktop
