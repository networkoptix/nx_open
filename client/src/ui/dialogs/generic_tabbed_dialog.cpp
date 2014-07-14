#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/warnings.h>

QnGenericTabbedDialog::QnGenericTabbedDialog(QWidget *parent /*= 0*/) :
    base_type(parent)
{

}

int QnGenericTabbedDialog::currentPage() const {
    if (!m_tabWidget)
        return 0;

    foreach (int page, m_pages.keys()) {
        if (m_tabWidget->currentWidget() == m_pages[page])
            return page;
    }
    return 0;
}

void QnGenericTabbedDialog::setCurrentPage(int page) {
    if (!m_tabWidget || !m_pages.contains(page))
        return;
    m_tabWidget->setCurrentWidget(m_pages[page]);
}

void QnGenericTabbedDialog::reject() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->discard())
            return;

    loadData();
    base_type::reject();
}

void QnGenericTabbedDialog::accept() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->confirm())
            return;

    submitData();
    base_type::accept();
}

void QnGenericTabbedDialog::loadData() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->updateFromSettings();
}

void QnGenericTabbedDialog::submitData() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->submitToSettings();
}

void QnGenericTabbedDialog::addPage(int key, QnAbstractPreferencesWidget *page, const QString &title) {
    if (!m_tabWidget)
        initializeTabWidget();

    if (!m_tabWidget) 
        return;

    if (m_pages[key]) {
        qnWarning("QnGenericTabbedDialog '%1' already contains %2 as %3", metaObject()->className(), page->metaObject()->className(), key);
        return;
    }

    m_pages[key] = page;
    m_tabWidget->addTab(page, title);
}

void QnGenericTabbedDialog::setTabWidget(QTabWidget *tabWidget) {
    if(m_tabWidget.data() == tabWidget)
        return;

    m_tabWidget = tabWidget;
}

void QnGenericTabbedDialog::initializeTabWidget() {
    if(m_tabWidget)
        return; /* Already initialized with a direct call to setTabWidget in derived class's constructor. */

    QList<QTabWidget *> tabWidgets = findChildren<QTabWidget *>();
    if(tabWidgets.empty()) {
        qnWarning("QnGenericTabbedDialog '%1' doesn't have a QTabWidget.", metaObject()->className());
        return;
    }
    if(tabWidgets.size() > 1) {
        qnWarning("QnGenericTabbedDialog '%1' has several QTabWidgets.", metaObject()->className());
        return;
    }

    setTabWidget(tabWidgets[0]);
}
