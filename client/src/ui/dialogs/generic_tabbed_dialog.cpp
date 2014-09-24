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

    foreach (const Page &page, m_pages) {
        if (m_tabWidget->currentWidget() == page.widget)
            return page.key;
    }
    return 0;
}

void QnGenericTabbedDialog::setCurrentPage(int key) {
    if (!m_tabWidget)
        return;

    foreach (const Page &page, m_pages) {
        if (page.key != key)
            continue;
        m_tabWidget->setCurrentWidget(page.widget);
        break;
    }
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
    foreach(const Page &page, m_pages)
        page.widget->updateFromSettings();
}

void QnGenericTabbedDialog::submitData() {
    foreach(const Page &page, m_pages)
        page.widget->submitToSettings();
}

void QnGenericTabbedDialog::addPage(int key, QnAbstractPreferencesWidget *page, const QString &title) {
    if (!m_tabWidget)
        initializeTabWidget();

    if (!m_tabWidget) 
        return;

    foreach(const Page &page, m_pages) {
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
    foreach(const Page &page, m_pages)
        if (!page.widget->confirm())
            return false;
    return true;
}

bool QnGenericTabbedDialog::discard() const {
    foreach(const Page &page, m_pages)
        if (!page.widget->discard())
            return false;
    return true;
}

bool QnGenericTabbedDialog::hasChanges() const {
    foreach(const Page &page, m_pages)
        if (page.widget->hasChanges())
            return true;
    return false;
}

QList<QnGenericTabbedDialog::Page> QnGenericTabbedDialog::modifiedPages() const {
    QList<QnGenericTabbedDialog::Page> result;
    foreach(const Page &page, m_pages)
        if (page.widget->hasChanges())
            result << page;
    return result;
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

    QStringList details;
    foreach(const Page &page, modifiedPages())
        details << tr("* %1").arg(page.title);

    QMessageBox::StandardButton btn =  QMessageBox::question(this,
        tr("Confirm exit"),
        tr("Unsaved changes will be lost. Save the following pages?\n") + details.join(L'\n'),
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

