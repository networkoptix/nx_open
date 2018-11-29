#include "animated_tab_widget.h"

#include <utils/common/scoped_value_rollback.h>

#include <nx/vms/client/desktop/common/widgets/transitioner_widget.h>

namespace nx::vms::client::desktop {

AnimatedTabWidget::AnimatedTabWidget(QWidget* parent):
    AnimatedTabWidget(nullptr, parent)
{
}

AnimatedTabWidget::AnimatedTabWidget(QTabBar* tabBar, QWidget* parent) :
    base_type(tabBar, parent),
    m_transitioner(new TransitionerWidget(this))
{
    m_transitioner->hide();
    connect(this, &QTabWidget::currentChanged, this, &AnimatedTabWidget::handleCurrentChanged);

    connect(m_transitioner, &TransitionerWidget::finished, this,
        [this]()
        {
            if (m_tabsParent)
                m_tabsParent->show();

            m_transitioner->hide();
        });
}

void AnimatedTabWidget::handleCurrentChanged()
{
    const auto newTab = currentWidget();

    // Current index can change due to tab removal, in that case current widget can remain the same.
    if (!newTab && !m_currentTab || newTab == m_currentTab)
        return;

    if (!isVisible())
    {
        m_currentTab = newTab;
        return;
    }

    const auto sourceRect = tabRect(m_currentTab);
    const auto targetRect = tabRect(newTab);

    const auto rect = sourceRect.united(targetRect);
    const auto sourceOffset = sourceRect.topLeft() - rect.topLeft();
    const auto targetOffset = targetRect.topLeft() - rect.topLeft();

    m_transitioner->setTarget(grabTab(newTab, rect.size(), targetOffset));

    if (!m_transitioner->running())
    {
        m_transitioner->setSource(grabTab(m_currentTab, rect.size(), sourceOffset));

        if (!m_tabsParent)
            m_tabsParent = newTab ? newTab->parentWidget() : m_currentTab->parentWidget();

        NX_ASSERT(m_tabsParent);
        m_tabsParent->hide();

        m_transitioner->setGeometry(rect);
        m_transitioner->show();
        m_transitioner->raise();
        m_transitioner->start();
    }

    m_currentTab = newTab;
}

QRect AnimatedTabWidget::tabRect(QWidget* tab) const
{
    if (!tab)
        return QRect();

    const auto tabLocalRect = tab->rect();
    return QRect(tab->mapTo(this, tabLocalRect.topLeft()), tabLocalRect.size());
}

QPixmap AnimatedTabWidget::grabTab(QWidget* tab, const QSize& size, const QPoint& offset) const
{
    if (!tab)
        return QPixmap();

    QPixmap pixmap(size * devicePixelRatio());
    pixmap.setDevicePixelRatio(devicePixelRatio());
    pixmap.fill(Qt::transparent);

    QnScopedTypedPropertyRollback<bool, QWidget> visibilityGuard(
        tab,
        [](QWidget* widget, bool value) { widget->setHidden(value); },
        [](const QWidget* widget) { return widget->isHidden(); },
        false);

    tab->render(&pixmap, offset, QRegion(), QWidget::DrawChildren);
    return pixmap;
}

} // namespace nx::vms::client::desktop
