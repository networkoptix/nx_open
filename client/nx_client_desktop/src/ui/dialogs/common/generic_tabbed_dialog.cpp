#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QPushButton>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/warnings.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace
{
    const int invalidPage = -1;
}

QnGenericTabbedDialog::QnGenericTabbedDialog(QWidget* parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    m_pages(),
    m_tabWidget(nullptr)
{
}

int QnGenericTabbedDialog::currentPage() const
{
    if (!m_tabWidget)
        return invalidPage;

    for(const Page &page: m_pages)
    {
        if (m_tabWidget->currentWidget() == page.widget)
            return page.key;
    }
    return invalidPage;
}

void QnGenericTabbedDialog::setCurrentPage(int key, bool adjust)
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

void QnGenericTabbedDialog::reject()
{
    if (!forcefullyClose())
        return;
    base_type::reject();
    emit dialogClosed();
}

void QnGenericTabbedDialog::accept()
{
    if (!canApplyChanges())
        return;

    if (hasChanges())
        applyChanges();
    base_type::accept();
    emit dialogClosed();
}

bool QnGenericTabbedDialog::forcefullyClose()
{
    /* Very rare outcome. */
    if (!canDiscardChanges())
        return false;

    hide();
    discardChanges();
    loadDataToUi();
    return true;
}

void QnGenericTabbedDialog::loadDataToUi()
{
    Qn::updateGuarded(this,
        [this]()
        {
            for(const Page &page: m_pages)
                page.widget->loadDataToUi();
        });

    retranslateUi();
    updateButtonBox();
}

void QnGenericTabbedDialog::retranslateUi()
{
    for(const Page &page: m_pages)
        page.widget->retranslateUi();
}

void QnGenericTabbedDialog::applyChanges()
{
    for(const Page &page: m_pages)
        if (page.enabled && page.visible && page.widget->hasChanges())
            page.widget->applyChanges();
    updateButtonBox();
}

void QnGenericTabbedDialog::discardChanges()
{
    for (const Page &page : m_pages)
        if (page.enabled && page.visible)
            page.widget->discardChanges();
}

void QnGenericTabbedDialog::addPage(int key, QnAbstractPreferencesWidget *page, const QString &title)
{
    if (!m_tabWidget)
        initializeTabWidget();

    NX_ASSERT(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");
    if (!m_tabWidget)
        return;

    for(const Page &page: m_pages) {
       if (page.key != key)
           continue;
        qnWarning("QnGenericTabbedDialog '%1' already contains %2 as %3", metaObject()->className(), page.widget->metaObject()->className(), key);
        return;
    }

    Page newPage;
    newPage.key = key;
    newPage.widget = page;
    newPage.title = title;
    m_pages << newPage;

    m_tabWidget->addTab(page, title);

    connect(page, &QnAbstractPreferencesWidget::hasChangesChanged, this, &QnGenericTabbedDialog::updateButtonBox);
}


bool QnGenericTabbedDialog::isPageVisible(int key) const
{
    for (const Page &page : m_pages)
        if (page.key == key)
            return page.visible;

    NX_ASSERT(false);
    return false;
}

void QnGenericTabbedDialog::setPageVisible(int key, bool visible)
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

    qnWarning("QnGenericTabbedDialog '%1' does not contain %2", metaObject()->className(), key);
}

void QnGenericTabbedDialog::setPageEnabled(int key, bool enabled)
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

    qnWarning("QnGenericTabbedDialog '%1' does not contain %2", metaObject()->className(), key);
    NX_ASSERT(false);
}

void QnGenericTabbedDialog::setTabWidget(QTabWidget* tabWidget)
{
    NX_ASSERT(!m_tabWidget);
    m_tabWidget = tabWidget;
}

void QnGenericTabbedDialog::initializeTabWidget()
{
    if (m_tabWidget)
        return; /* Already initialized with a direct call to setTabWidget in derived class's constructor. */

    auto tabWidgets = findChildren<QTabWidget*>();
    NX_ASSERT(tabWidgets.size() == 1, "Call setTabWidget() from the constructor.");
    if (tabWidgets.size() != 1)
        return;

    setTabWidget(tabWidgets[0]);
}

bool QnGenericTabbedDialog::canApplyChanges() const
{
    if (isReadOnly())
        return false;

    return all_of(m_pages, [](const Page &page)
    {
        return !page.enabled || !page.visible || page.widget->canApplyChanges();
    });
}

bool QnGenericTabbedDialog::canDiscardChanges() const
{
    if (isReadOnly())
        return true;

    return all_of(m_pages, [](const Page &page)
    {
        return !page.enabled || !page.visible || page.widget->canDiscardChanges();
    });
}

bool QnGenericTabbedDialog::hasChanges() const
{
    if (isReadOnly())
        return false;

    return any_of(m_pages, [](const Page &page)
    {
        return page.enabled && page.visible && page.widget->hasChanges();
    });
}

QList<QnGenericTabbedDialog::Page> QnGenericTabbedDialog::allPages() const
{
    return m_pages;
}

QList<QnGenericTabbedDialog::Page> QnGenericTabbedDialog::modifiedPages() const
{
    QList<QnGenericTabbedDialog::Page> result;
    for(const Page &page: m_pages)
        if (page.enabled && page.visible && page.widget->hasChanges())
            result << page;
    return result;
}

void QnGenericTabbedDialog::setReadOnlyInternal()
{
    for (const auto& page : m_pages)
        page.widget->setReadOnly(isReadOnly());
}

void QnGenericTabbedDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button)
{
    base_type::buttonBoxClicked(button);

    switch (button)
    {
    case QDialogButtonBox::Apply:
        if (canApplyChanges() && hasChanges())
        {
            applyChanges();
            updateButtonBox();
        }
        break;
    default:
        break;
    }
}

void QnGenericTabbedDialog::initializeButtonBox()
{
    base_type::initializeButtonBox();
    updateButtonBox();

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

void QnGenericTabbedDialog::updateButtonBox()
{
    if (isUpdating() || !buttonBox())
        return;

    bool changesPresent = hasChanges();
    bool canApply = !changesPresent || canApplyChanges();

    if (QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply))
        applyButton->setEnabled(!isReadOnly() && changesPresent && canApply);

    if (QPushButton *okButton = buttonBox()->button(QDialogButtonBox::Ok))
        okButton->setEnabled(isReadOnly() || canApply);
}

