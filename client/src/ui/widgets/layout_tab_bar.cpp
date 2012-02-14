#include "layout_tab_bar.h"
#include <QVariant>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <ui/style/skin.h>

Q_DECLARE_METATYPE(QnWorkbenchLayout *);

QnLayoutTabBar::QnLayoutTabBar(QWidget *parent):
    QTabBar(parent),
    m_workbench(NULL),
    m_submit(false),
    m_update(false)
{
    /* Set up defaults. */
    setMovable(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDrawBase(false);
    setShape(QTabBar::RoundedNorth);

    connect(this, SIGNAL(currentChanged(int)),      this, SLOT(at_currentChanged(int)));
    connect(this, SIGNAL(tabCloseRequested(int)),   this, SLOT(at_tabCloseRequested(int)));
    connect(this, SIGNAL(tabMoved(int, int)),       this, SLOT(at_tabMoved(int, int)));

    updateTabsClosable();
}

QnLayoutTabBar::~QnLayoutTabBar() {
    setWorkbench(NULL);
}

void QnLayoutTabBar::checkInvariants() const {
    assert(m_layouts.size() == count());

    if(m_workbench && m_submit && m_update) {
        assert(m_workbench->layouts() == m_layouts);
        assert(m_workbench->layoutIndex(m_workbench->currentLayout()) == currentIndex() || m_workbench->layoutIndex(m_workbench->currentLayout()) == -1);
    }
}

void QnLayoutTabBar::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench == workbench)
        return;

    if(m_workbench != NULL)
        disconnect(m_workbench, NULL, this, NULL);
    while(count() > 0)
        removeTab(count() - 1);
    m_submit = m_update = false;

    m_workbench = workbench;

    if(m_workbench != NULL) {
        at_workbench_layoutsChanged();
        at_workbench_currentLayoutChanged();

        connect(m_workbench, SIGNAL(aboutToBeDestroyed()),      this, SLOT(at_workbench_aboutToBeDestroyed()));
        connect(m_workbench, SIGNAL(layoutsChanged()),          this, SLOT(at_workbench_layoutsChanged()));
        connect(m_workbench, SIGNAL(currentLayoutChanged()),    this, SLOT(at_workbench_currentLayoutChanged()));

        m_submit = m_update = true;
    }

    checkInvariants();
}

QnWorkbench *QnLayoutTabBar::workbench() const {
    return m_workbench;
}

void QnLayoutTabBar::updateTabsClosable() {
    setTabsClosable(count() > 1);
}

void QnLayoutTabBar::submitCurrentLayout() {
    if(!m_submit)
        return;
    
    QnScopedValueRollback<bool> guard(&m_update, false);
    m_workbench->setCurrentLayout(currentIndex() == -1 ? NULL : m_layouts[currentIndex()]);

    guard.rollback();
    checkInvariants();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLayoutTabBar::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnLayoutTabBar::at_workbench_layoutsChanged() {
    if(!m_update)
        return;

    const QList<QnWorkbenchLayout *> &layouts = m_workbench->layouts();
    if(m_layouts == layouts)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    for(int i = 0; i < layouts.size(); i++) {
        int index = m_layouts.indexOf(layouts[i]);
        if(index == -1) {
            m_layouts.insert(i, layouts[i]);
            insertTab(i, layouts[i]->name());
        } else {
            moveTab(index, i);
        }
    }

    while(count() > layouts.size())
        removeTab(count() - 1);

    /* Current layout may have changed. Sync. */
    at_workbench_currentLayoutChanged();

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_workbench_currentLayoutChanged() {
    if(!m_update)
        return;

    /* It is important to check indices equality because it will also work
     * for invalid current index (-1). */
    int newCurrentIndex = m_workbench->layoutIndex(m_workbench->currentLayout());
    if(currentIndex() == newCurrentIndex)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);
    setCurrentIndex(newCurrentIndex);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_layout_nameChanged() {
    QnWorkbenchLayout *layout = checked_cast<QnWorkbenchLayout *>(sender());

    setTabText(m_layouts.indexOf(layout), layout->name());
}

void QnLayoutTabBar::tabInserted(int index) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QString name;
    if(m_layouts.size() != count()) { /* Not inserted yet, allocate new one. It will be deleted with this tab bar. */
        QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);

        /* Create name for new layout. */
        QSet<QString> layoutNames;
        foreach(QnWorkbenchLayout *layout, m_layouts)
            layoutNames.insert(layout->name());

        for(int i = 1;; i++) {
            name = i == 1 ? tr("New layout") : tr("New layout %1").arg(i);
            if(!layoutNames.contains(name))
                break;
        }

        m_layouts.insert(index, layout);
    }

    QnWorkbenchLayout *layout = m_layouts[index];
    connect(layout, SIGNAL(nameChanged()),          this, SLOT(at_layout_nameChanged()));

    if(!name.isNull())
        layout->setName(name); /* It is important to set the name after connecting so that the name change signal is delivered to us. */

    updateTabsClosable();
    if(m_submit)
        m_workbench->insertLayout(layout, index);
    submitCurrentLayout();

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::tabRemoved(int index) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QnWorkbenchLayout *layout = m_layouts[index];
    disconnect(layout, NULL, this, NULL);

    m_layouts.removeAt(index);
    updateTabsClosable();
    submitCurrentLayout();

    if(m_submit)
        m_workbench->removeLayout(layout);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_tabMoved(int from, int to) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QnWorkbenchLayout *layout = m_layouts[from];
    m_layouts.move(from, to);

    if(m_submit)
        m_workbench->moveLayout(layout, to);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_tabCloseRequested(int index) {
    if (count() <= 1)
        return; /* Don't remove the last tab. */

    emit closeRequested(m_layouts[index]);
}

void QnLayoutTabBar::at_currentChanged(int index) {
    Q_UNUSED(index);

    if(m_layouts.size() != count())
        return; /* Was called from add/remove function before our handler was able to sync, current layout will be updated later. */

    submitCurrentLayout();

    checkInvariants();
}

