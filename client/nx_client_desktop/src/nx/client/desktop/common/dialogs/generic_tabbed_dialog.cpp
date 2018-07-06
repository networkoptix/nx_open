#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/warnings.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace {

const int kInvalidPage = -1;

} // namespace

namespace nx {
namespace client {
namespace desktop {

GenericTabbedDialog::GenericTabbedDialog(QWidget* parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_pages(),
    m_tabWidget(nullptr)
{
}

int GenericTabbedDialog::currentPage() const
{
    if (!m_tabWidget)
        return kInvalidPage;

    for(const Page &page: m_pages)
    {
        if (m_tabWidget->currentWidget() == page.widget)
            return page.key;
    }
    return kInvalidPage;
}

void GenericTabbedDialog::setCurrentPage(int key, bool adjust)
{
    NX_ASSERT(m_tabWidget);
    NX_ASSERT(!m_pages.isEmpty());
    if (!m_tabWidget || m_pages.isEmpty())
        return;

    auto page = std::find_if(m_pages.cbegin(), m_pages.cend(),
        [key](const Page& page)
        {
            return page.key == key;
        });

    NX_ASSERT(page != m_pages.cend());
    if (!adjust)
    {
        if (page == m_pages.cend())
            return;

        NX_ASSERT(page->isValid());
        if (page->isValid())
            m_tabWidget->setCurrentWidget(page->widget);
        return;
    }

    auto nearestPage = m_pages.cend();
    for (auto p = m_pages.cbegin(); p != m_pages.cend(); ++p)
    {
        if (!p->isValid())
            continue;
        if (nearestPage == m_pages.cend()
            || qAbs(nearestPage->key - key) > qAbs(p->key - key))
                nearestPage = p;
    }
    NX_ASSERT(nearestPage != m_pages.cend());
    if (nearestPage != m_pages.cend())
        m_tabWidget->setCurrentWidget(nearestPage->widget);
}

void GenericTabbedDialog::addPage(int key, QWidget *page, const QString &title)
{
    if (!m_tabWidget)
        initializeTabWidget();

    NX_ASSERT(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");
    if (!m_tabWidget)
        return;

    for(const Page &page: m_pages) {
       if (page.key != key)
           continue;
        qnWarning("GenericTabbedDialog '%1' already contains %2 as %3", metaObject()->className(), page.widget->metaObject()->className(), key);
        return;
    }

    Page newPage;
    newPage.key = key;
    newPage.widget = page;
    newPage.title = title;
    m_pages << newPage;

    m_tabWidget->addTab(page, title);
}


bool GenericTabbedDialog::isPageVisible(int key) const
{
    for (const Page &page : m_pages)
        if (page.key == key)
            return page.visible;

    NX_ASSERT(false);
    return false;
}

void GenericTabbedDialog::setPageVisible(int key, bool visible)
{
    NX_ASSERT(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");

    if (!m_tabWidget)
        return;

    int indexToInsert = 0;
    for (Page &page : m_pages)
    {
        /* Checking last visible widget before current. */
        if (page.key != key)
        {
            if (page.visible)
            {
                int prevIndex = m_tabWidget->indexOf(page.widget);
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
            int indexToRemove = m_tabWidget->indexOf(page.widget);
            NX_ASSERT(indexToRemove >= 0);
            if (indexToRemove >= 0)
                m_tabWidget->removeTab(indexToRemove);
        }
        return;
    }

    qnWarning("GenericTabbedDialog '%1' does not contain %2", metaObject()->className(), key);
}

void GenericTabbedDialog::setPageEnabled(int key, bool enabled)
{
    NX_ASSERT(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");

    if (!m_tabWidget)
        return;

    for (Page& page : m_pages)
    {
        if (page.key != key)
            continue;

        if (page.enabled == enabled)
            return;

        page.enabled = enabled;

        int index = m_tabWidget->indexOf(page.widget);
        NX_ASSERT(index >= 0);
        if (index < 0)
            return;

        m_tabWidget->setTabEnabled(index, enabled);
        return;
    }

    qnWarning("GenericTabbedDialog '%1' does not contain %2", metaObject()->className(), key);
    NX_ASSERT(false);
}

void GenericTabbedDialog::setTabWidget(QTabWidget* tabWidget)
{
    NX_ASSERT(!m_tabWidget);
    m_tabWidget = tabWidget;
}

void GenericTabbedDialog::initializeTabWidget()
{
    if (m_tabWidget)
        return; /* Already initialized with a direct call to setTabWidget in derived class's constructor. */

    auto tabWidgets = findChildren<QTabWidget*>();
    NX_ASSERT(tabWidgets.size() == 1, "Call setTabWidget() from the constructor.");
    if (tabWidgets.size() != 1)
        return;

    setTabWidget(tabWidgets[0]);
}

QList<GenericTabbedDialog::Page> GenericTabbedDialog::allPages() const
{
    return m_pages;
}

void GenericTabbedDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();

    QWidget* last = buttonBox();
    for (;;)
    {
        auto next = last->nextInFocusChain();
        if (!next || next == buttonBox() || next->parentWidget() != buttonBox())
            break;

        last = next;
    }

    auto tabBar = m_tabWidget->tabBar();

    setTabOrder(last, tabBar);
    tabBar->setFocus();
}


} // namespace desktop
} // namespace client
} // namespace nx
