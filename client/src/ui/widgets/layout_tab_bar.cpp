#include "layout_tab_bar.h"

#include <QVariant>

#include <utils/common/warnings.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/style/skin.h>

Q_DECLARE_METATYPE(QnWorkbenchLayout *);

QnLayoutTabBar::QnLayoutTabBar(QWidget *parent):
    QTabBar(parent),
    m_context(NULL),
    m_submit(false),
    m_update(false)
{
    /* Set up defaults. */
    setMovable(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDrawBase(false);
    setShape(QTabBar::RoundedNorth);
    setTabsClosable(true);

    connect(this, SIGNAL(currentChanged(int)),      this, SLOT(at_currentChanged(int)));
    connect(this, SIGNAL(tabCloseRequested(int)),   this, SLOT(at_tabCloseRequested(int)));
    connect(this, SIGNAL(tabMoved(int, int)),       this, SLOT(at_tabMoved(int, int)));
}

QnLayoutTabBar::~QnLayoutTabBar() {
    setContext(NULL);
}

void QnLayoutTabBar::checkInvariants() const {
    assert(m_layouts.size() == count());

    if(workbench() && m_submit && m_update) {
        assert(workbench()->layouts() == m_layouts);
        assert(workbench()->layoutIndex(workbench()->currentLayout()) == currentIndex() || workbench()->layoutIndex(workbench()->currentLayout()) == -1);
    }
}

QnWorkbenchContext *QnLayoutTabBar::context() const {
    return m_context;
}

QnWorkbench *QnLayoutTabBar::workbench() const {
    return m_context ? m_context->workbench() : NULL;
}

QnWorkbenchLayoutSnapshotManager *QnLayoutTabBar::snapshotManager() const {
    return m_context ? m_context->snapshotManager() : NULL;
}

void QnLayoutTabBar::setContext(QnWorkbenchContext *context) {
    if(m_context == context)
        return;

    if(m_context != NULL) {
        disconnect(m_context, NULL, this, NULL);
        disconnect(workbench(), NULL, this, NULL);
        disconnect(snapshotManager(), NULL, this, NULL);
    }
    while(count() > 0)
        removeTab(count() - 1);
    m_submit = m_update = false;

    m_context = context;

    if(m_context != NULL) {
        at_workbench_layoutsChanged();
        at_workbench_currentLayoutChanged();

        connect(m_context,          SIGNAL(aboutToBeDestroyed()),                       this, SLOT(at_context_aboutToBeDestroyed()));
        connect(workbench(),        SIGNAL(layoutsChanged()),                           this, SLOT(at_workbench_layoutsChanged()));
        connect(workbench(),        SIGNAL(currentLayoutChanged()),                     this, SLOT(at_workbench_currentLayoutChanged()));
        connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),   this, SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));

        m_submit = m_update = true;
    }

    checkInvariants();
}

void QnLayoutTabBar::submitCurrentLayout() {
    if(!m_submit)
        return;
    
    QnScopedValueRollback<bool> guard(&m_update, false);
    workbench()->setCurrentLayout(currentIndex() == -1 ? NULL : m_layouts[currentIndex()]);

    guard.rollback();
    checkInvariants();
}

QString QnLayoutTabBar::layoutText(QnWorkbenchLayout *layout) const {
    if(layout == NULL)
        return QString();

    QnLayoutResourcePtr resource = layout->resource();
    return layout->name() + (snapshotManager()->isModified(resource) ? QLatin1String("*") : QString());
}

void QnLayoutTabBar::updateTabText(QnWorkbenchLayout *layout) {
    setTabText(m_layouts.indexOf(layout), layoutText(layout));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLayoutTabBar::at_context_aboutToBeDestroyed() {
    setContext(NULL);
}

void QnLayoutTabBar::at_workbench_layoutsChanged() {
    if(!m_update)
        return;

    const QList<QnWorkbenchLayout *> &layouts = workbench()->layouts();
    if(m_layouts == layouts)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);

    for(int i = 0; i < layouts.size(); i++) {
        int index = m_layouts.indexOf(layouts[i]);
        if(index == -1) {
            m_layouts.insert(i, layouts[i]);
            insertTab(i, layoutText(layouts[i]));
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
    int newCurrentIndex = workbench()->layoutIndex(workbench()->currentLayout());
    if(currentIndex() == newCurrentIndex)
        return;

    QnScopedValueRollback<bool> guard(&m_submit, false);
    setCurrentIndex(newCurrentIndex);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_layout_nameChanged() {
    QnWorkbenchLayout *layout = checked_cast<QnWorkbenchLayout *>(sender());

    updateTabText(layout);
}

void QnLayoutTabBar::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource) {
    updateTabText(QnWorkbenchLayout::layout(resource));
}

void QnLayoutTabBar::tabInserted(int index) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QString name;
    if(m_layouts.size() != count()) { /* Not inserted yet, allocate new one. It will be deleted with this tab bar. */
        QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);
        m_layouts.insert(index, layout);
        name = tabText(index);
    }

    QnWorkbenchLayout *layout = m_layouts[index];
    connect(layout, SIGNAL(nameChanged()),          this, SLOT(at_layout_nameChanged()));

    if(!name.isNull())
        layout->setName(name); /* It is important to set the name after connecting so that the name change signal is delivered to us. */

    if(m_submit)
        workbench()->insertLayout(layout, index);
    submitCurrentLayout();

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::tabRemoved(int index) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QnWorkbenchLayout *layout = m_layouts[index];
    disconnect(layout, NULL, this, NULL);

    m_layouts.removeAt(index);
    submitCurrentLayout();

    if(m_submit)
        workbench()->removeLayout(layout);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_tabMoved(int from, int to) {
    QnScopedValueRollback<bool> guard(&m_update, false);

    QnWorkbenchLayout *layout = m_layouts[from];
    m_layouts.move(from, to);

    if(m_submit)
        workbench()->moveLayout(layout, to);

    guard.rollback();
    checkInvariants();
}

void QnLayoutTabBar::at_tabCloseRequested(int index) {
    emit closeRequested(m_layouts[index]);
}

void QnLayoutTabBar::at_currentChanged(int index) {
    Q_UNUSED(index);

    if(m_layouts.size() != count())
        return; /* Was called from add/remove function before our handler was able to sync, current layout will be updated later. */

    submitCurrentLayout();

    checkInvariants();
}

