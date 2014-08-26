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
    if (!tryClose(false))
        return;

    loadData();
    base_type::reject();
}

void QnGenericTabbedDialog::accept() {
    if (!confirm())
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

bool QnGenericTabbedDialog::confirm() const {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->confirm())
            return false;
    return true;
}

bool QnGenericTabbedDialog::discard() const {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->discard())
            return false;
    return true;
}

bool QnGenericTabbedDialog::hasChanges() const {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (page->hasChanges())
            return true;
    return false;
}


bool QnGenericTabbedDialog::tryClose(bool force) {
    if (force) {
        if (!discard())
            return false;   //TODO: #dklychkov and what then? see QnWorkbenchConnectHandler
        loadData();
        hide();
        return true;
    }

    if (isHidden() || !hasChanges())
        return true;

    QMessageBox::StandardButton btn =  QMessageBox::question(this,
        tr("Confirm exit"),
        tr("Unsaved changes will be lost. Save?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Cancel);

    switch (btn) {
    case QMessageBox::Yes:
        if (!confirm())
            return false;   // Cancel was pressed in the confirmation dialog

        submitData();
        break;
    case QMessageBox::No:
        loadData();
        break;
    default:
        return false;   // Cancel was pressed
    }

    return true;
}

