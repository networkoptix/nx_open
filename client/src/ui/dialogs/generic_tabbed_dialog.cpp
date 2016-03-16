#include "generic_tabbed_dialog.h"

#include <QtWidgets/QTabWidget>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <ui/widgets/settings/abstract_preferences_widget.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace {
    const int invalidPage = -1;
}

QnGenericTabbedDialog::QnGenericTabbedDialog(QWidget *parent /* = 0 */, Qt::WindowFlags windowFlags /* = 0 */) 
    : base_type(parent, windowFlags)
    , m_pages()
    , m_tabWidget(nullptr)
    , m_readOnly(false)
    , m_updating(false)
{

}

int QnGenericTabbedDialog::currentPage() const {
    if (!m_tabWidget)
        return invalidPage;

    for(const Page &page: m_pages) {
        if (m_tabWidget->currentWidget() == page.widget)
            return page.key;
    }
    return invalidPage;
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
    if (!forcefullyClose())
        return;
    base_type::reject();
    emit dialogClosed();
}

void QnGenericTabbedDialog::accept() {
    if (!canApplyChanges())
        return;

    applyChanges();
    base_type::accept();
    emit dialogClosed();
}

bool QnGenericTabbedDialog::forcefullyClose()  {
    /* Very rare outcome. */
    if (!canDiscardChanges())
        return false;

    loadDataToUi();
    hide();
    return true;
}

void QnGenericTabbedDialog::loadDataToUi() {
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        for(const Page &page: m_pages)
            page.widget->loadDataToUi();
    }
    retranslateUi();
    updateButtonBox();
}

void QnGenericTabbedDialog::retranslateUi() {
    for(const Page &page: m_pages)
        page.widget->retranslateUi();
}

void QnGenericTabbedDialog::applyChanges() {
    for(const Page &page: m_pages)
        page.widget->applyChanges();
    updateButtonBox();
}

void QnGenericTabbedDialog::addPage(int key, QnAbstractPreferencesWidget *page, const QString &title) {
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


void QnGenericTabbedDialog::setPageEnabled(int key, bool enabled) {
    NX_ASSERT(m_tabWidget, Q_FUNC_INFO, "tab widget must exist here");

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

bool QnGenericTabbedDialog::canApplyChanges() {
    return all_of(m_pages, [](const Page &page){
        return page.widget->canApplyChanges();
    });
}

bool QnGenericTabbedDialog::canDiscardChanges() {
    return all_of(m_pages, [](const Page &page){
        return page.widget->canDiscardChanges();
    });
}

bool QnGenericTabbedDialog::hasChanges() const {
    return any_of(m_pages, [](const Page &page){
        return page.widget->hasChanges();
    });
}

QList<QnGenericTabbedDialog::Page> QnGenericTabbedDialog::allPages() const {
    return m_pages;
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

void QnGenericTabbedDialog::buttonBoxClicked(QDialogButtonBox::StandardButton button) {
    base_type::buttonBoxClicked(button);

    switch (button) {
    case QDialogButtonBox::Apply:
        if (canApplyChanges())
            applyChanges();
        break;
    default:
        break;
    }
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


