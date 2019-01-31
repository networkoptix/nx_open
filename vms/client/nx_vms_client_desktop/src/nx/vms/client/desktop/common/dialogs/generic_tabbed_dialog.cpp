#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>

namespace nx::vms::client::desktop {

GenericTabbedDialog::GenericTabbedDialog(QWidget* parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags)
{
}

int GenericTabbedDialog::currentPage() const
{
    if (!m_tabWidget)
        return kInvalidPage;

    const auto page = std::find_if(m_pages.cbegin(), m_pages.cend(),
        [w = m_tabWidget->currentWidget()](const Page& page) { return page.widget == w; });

    return page == m_pages.cend()
        ? kInvalidPage
        : page->key;
}

void GenericTabbedDialog::setCurrentPage(int key)
{
    NX_ASSERT(m_tabWidget);
    NX_ASSERT(!m_pages.isEmpty());
    if (!m_tabWidget || m_pages.isEmpty())
        return;

    const auto page = findPage(key);
    NX_ASSERT(page != m_pages.cend());
    if (page == m_pages.cend())
        return;

    NX_ASSERT(page->isValid());
    if (page->isValid())
        m_tabWidget->setCurrentWidget(page->widget);
}

void GenericTabbedDialog::addPage(int key, QWidget* widget, const QString& title)
{
    if (!m_tabWidget)
        initializeTabWidget();

    NX_ASSERT(m_tabWidget, "tab widget must exist here");
    if (!m_tabWidget)
        return;

    const auto existing = findPage(key);
    NX_ASSERT(existing == m_pages.cend(),
        lm("GenericTabbedDialog '%1' already contains %2 as %3").args(
            metaObject()->className(),
            existing->widget->metaObject()->className(),
            key));

    Page newPage;
    newPage.key = key;
    newPage.widget = widget;
    newPage.title = title;
    m_pages << newPage;

    m_tabWidget->addTab(widget, title);
}

bool GenericTabbedDialog::isPageVisible(int key) const
{
    const auto page = findPage(key);
    NX_ASSERT(page != m_pages.cend());
    return page == m_pages.cend()
        ? false
        : page->visible;
}

void GenericTabbedDialog::setPageVisible(int key, bool visible)
{
    NX_ASSERT(m_tabWidget, "tab widget must exist here");

    if (!m_tabWidget)
        return;

    int indexToInsert = 0;
    for (auto& page: m_pages)
    {
        /* Checking last visible widget before current. */
        if (page.key != key)
        {
            if (page.visible)
            {
                const int prevIndex = m_tabWidget->indexOf(page.widget);
                if (prevIndex >= 0)
                    indexToInsert = prevIndex + 1;
            }
            continue;
        }

        if (page.visible == visible)
            return;
        page.visible = visible;

        if (visible)
        {
            m_tabWidget->insertTab(indexToInsert, page.widget, page.title);
        }
        else
        {
            const int indexToRemove = m_tabWidget->indexOf(page.widget);
            NX_ASSERT(indexToRemove >= 0);
            if (indexToRemove >= 0)
                m_tabWidget->removeTab(indexToRemove);
        }
        return;
    }

    NX_ASSERT(false, lm("GenericTabbedDialog '%1' does not contain %2").args(
        metaObject()->className(), key));
}

void GenericTabbedDialog::setPageEnabled(int key, bool enabled)
{
    NX_ASSERT(m_tabWidget, "tab widget must exist here");

    if (!m_tabWidget)
        return;

    auto page = findPage(key);
    NX_ASSERT(page != m_pages.end(), lm("GenericTabbedDialog '%1' does not contain %2").args(
        metaObject()->className(), key));

    if (page == m_pages.end() || page->enabled == enabled)
        return;

    page->enabled = enabled;
    const int index = m_tabWidget->indexOf(page->widget);
    NX_ASSERT(index >= 0);
    if (index < 0)
        return;

    m_tabWidget->setTabEnabled(index, enabled);
}

void GenericTabbedDialog::setTabWidget(QTabWidget* tabWidget)
{
    NX_ASSERT(!m_tabWidget);
    m_tabWidget = tabWidget;
}

void GenericTabbedDialog::initializeTabWidget()
{
    // Check if already initialized with a direct call in derived class's constructor.
    if (m_tabWidget)
        return;

    auto tabWidgets = findChildren<QTabWidget*>();
    NX_ASSERT(tabWidgets.size() == 1, "Call setTabWidget() from the constructor.");
    if (tabWidgets.size() != 1)
        return;

    setTabWidget(tabWidgets[0]);
}

QList<GenericTabbedDialog::Page>::iterator GenericTabbedDialog::findPage(int key)
{
    return std::find_if(m_pages.begin(), m_pages.end(),
        [key](const Page& page) { return page.key == key; });
}

QList<GenericTabbedDialog::Page>::const_iterator GenericTabbedDialog::findPage(int key) const
{
    return std::find_if(m_pages.cbegin(), m_pages.cend(),
        [key](const Page& page) { return page.key == key; });
}

void GenericTabbedDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();

    QWidget* last = buttonBox();
    for (;;)
    {
        const auto next = last->nextInFocusChain();
        if (!next || next == buttonBox() || next->parentWidget() != buttonBox())
            break;

        last = next;
    }

    auto tabBar = m_tabWidget->tabBar();

    setTabOrder(last, tabBar);
    tabBar->setFocus();
}

} // namespace nx::vms::client::desktop
