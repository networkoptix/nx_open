#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabWidget>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>

using boost::algorithm::any_of;

QnGenericTabbedDialog::QnGenericTabbedDialog(QWidget *parent /* = 0 */) 
    : base_type(parent)
    , m_pages()
    , m_tabWidget(nullptr)
    , m_readOnly(false)
    , m_updating(false)
{

}

int QnGenericTabbedDialog::currentPage() const {
    if (!m_tabWidget)
        return 0;

    for(const Page &page: m_pages) {
        if (m_tabWidget->currentWidget() == page.widget)
            return page.key;
    }
    return 0;
}

void QnGenericTabbedDialog::setCurrentPage(int key) {
    if (!m_tabWidget)
        return;

    for(const Page &page: m_pages) {
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
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        for(const Page &page: m_pages)
            page.widget->updateFromSettings();
    }
    updateButtonBox();
}

void QnGenericTabbedDialog::submitData() {
    for(const Page &page: m_pages)
        page.widget->submitToSettings();
    updateButtonBox();
}

void QnGenericTabbedDialog::addPage(int key, QnAbstractPreferencesWidget *page, const QString &title) {
    if (!m_tabWidget)
        initializeTabWidget();

    Q_ASSERT_X(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");
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


void QnGenericTabbedDialog::setPageEnabled(int key, bool enabled) {
    Q_ASSERT_X(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");

    if (!m_tabWidget) 
        return;

    for(const Page &page: m_pages) {
        if (page.key != key)
            continue;

        int index = m_tabWidget->indexOf(page.widget);
        if (index < 0)
            return;

        m_tabWidget->setTabEnabled(index, enabled);
        return;
    }

    qnWarning("QnGenericTabbedDialog '%1' does not contain %2", metaObject()->className(), key);
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

bool QnGenericTabbedDialog::confirm() {
    for(const Page &page: m_pages)
        if (!page.widget->confirm())
            return false;
    return true;
}

bool QnGenericTabbedDialog::discard() {
    for(const Page &page: m_pages)
        if (!page.widget->discard())
            return false;
    return true;
}

bool QnGenericTabbedDialog::hasChanges() const {
    return any_of(m_pages, [](const Page &page){
        return page.widget->hasChanges();
    });
}

QList<QnGenericTabbedDialog::Page> QnGenericTabbedDialog::modifiedPages() const {
    QList<QnGenericTabbedDialog::Page> result;
    for(const Page &page: m_pages)
        if (page.widget->hasChanges())
            result << page;
    return result;
}

bool QnGenericTabbedDialog::isReadOnly() const {
    return m_readOnly;
}

void QnGenericTabbedDialog::setReadOnly( bool readOnly ) {
    for(const Page &page: m_pages)
        page.widget->setReadOnly(readOnly);
    m_readOnly = readOnly;
}

QString QnGenericTabbedDialog::confirmMessageTitle() const {
    return tr("Confirm exit");
}

QString QnGenericTabbedDialog::confirmMessageText() const {
    QStringList details;
    for(const Page &page: modifiedPages())
        details << tr("* %1").arg(page.title);
    return tr("Unsaved changes will be lost. Save the following pages?") + L'\n' + details.join(L'\n');
}

void QnGenericTabbedDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button) {
    base_type::buttonBoxClicked(button);

    switch (button) {
    case QDialogButtonBox::Apply:
        if (!confirm())
            return;
        submitData();
        break;
    default:
        break;
    }
}

bool QnGenericTabbedDialog::tryClose(bool force) {
    if (!discard())
        return false;   //TODO: #dklychkov and what then? see QnWorkbenchConnectHandler

    if (force) {
        loadData();
        hide();
        return true;
    }

    if (isHidden() || !hasChanges())
        return true;

    QMessageBox::StandardButton btn =  QMessageBox::question(this,
        confirmMessageTitle(),
        confirmMessageText(),
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

void QnGenericTabbedDialog::forcedUpdate() {
    retranslateUi();
    loadData();
}

void QnGenericTabbedDialog::retranslateUi() {
    for(const Page &page: m_pages)
        page.widget->retranslateUi();
}

void QnGenericTabbedDialog::initializeButtonBox() {
    base_type::initializeButtonBox();
    updateButtonBox();
}


void QnGenericTabbedDialog::updateButtonBox() {
    if (m_updating || !buttonBox())
        return;

    if (QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply))
        applyButton->setEnabled(!m_readOnly && hasChanges());
}
