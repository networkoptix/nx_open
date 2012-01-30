#include "layout_tab_bar.h"
#include <QVariant>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/warnings.h>
#include <ui/style/skin.h>

Q_DECLARE_METATYPE(QnWorkbenchLayout *);

QnLayoutTabBar::QnLayoutTabBar(QWidget *parent):
    QTabBar(parent),
    m_currentLayout(NULL)
{
    /* Set up defaults. */
    setMovable(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDrawBase(false);
    setShape(QTabBar::RoundedNorth);

    connect(this, SIGNAL(currentChanged(int)), this, SLOT(at_currentChanged(int)));
    connect(this, SIGNAL(tabCloseRequested(int)), this, SLOT(at_tabCloseRequested(int)));
    connect(this, SIGNAL(tabMoved(int, int)), this, SLOT(at_tabMoved(int, int)));

    updateTabsClosable();
}

QnLayoutTabBar::~QnLayoutTabBar() {
    return;
}

void QnLayoutTabBar::checkInvariants() const {
    assert(m_layouts.size() == count());
}

QnWorkbenchLayout *QnLayoutTabBar::layout(int index) const {
    if(index < 0 || index >= count()) {
        qnWarning("Layout index out of bounds: %1 not in [%2, %3).", index, 0, count());
        return NULL;
    }

    return m_layouts[index];
}

void QnLayoutTabBar::setCurrentLayout(QnWorkbenchLayout *layout) {
    int index = indexOf(layout);
    if(index == -1) {
        qnWarning("Given layout does not belong to this tab bar.");
        return;
    }

    setCurrentIndex(index);
}

void QnLayoutTabBar::addLayout(QnWorkbenchLayout *layout) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    m_layouts.push_back(layout);
    addTab(Skin::icon(QLatin1String("decorations/square-view.png")), tr("Tab"));

    checkInvariants();
}

void QnLayoutTabBar::insertLayout(int index, QnWorkbenchLayout *layout) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(index < 0 || index > count()) {
        qnWarning("Layout index out of bounds: %1 not in [%2, %3].", index, 0, count());
        return;
    }

    m_layouts.insert(index, layout);
    insertTab(index, Skin::icon(QLatin1String("decorations/square-view.png")), tr("Tab"));

    checkInvariants();
}

void QnLayoutTabBar::removeLayout(QnWorkbenchLayout *layout) {
    int index = indexOf(layout);
    if(index == -1) {
        qnWarning("Given layout does not belong to this tab bar.");
        return;
    }

    removeTab(index);

    checkInvariants();
}

int QnLayoutTabBar::indexOf(QnWorkbenchLayout *layout) const {
    return m_layouts.indexOf(layout);
}

void QnLayoutTabBar::updateTabsClosable() {
    setTabsClosable(count() > 1);
}

void QnLayoutTabBar::updateCurrentLayout() {
    checkInvariants();

    QnWorkbenchLayout *currentLayout = count() == 0 ? NULL : m_layouts[currentIndex()];
    if(m_currentLayout == currentLayout)
        return;

    m_currentLayout = currentLayout;
    emit currentChanged(m_currentLayout);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLayoutTabBar::tabInserted(int index) {
    if(m_layouts.size() != count()) /* Not inserted yet, allocate new one. It will be deleted with this tab bar. */
        m_layouts.insert(index, new QnWorkbenchLayout(this));

    checkInvariants();

    QnWorkbenchLayout *layout = m_layouts[index];
    connect(layout, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_layout_aboutToBeDestroyed()));

    updateTabsClosable();
    updateCurrentLayout();

    emit layoutInserted(layout);
}

void QnLayoutTabBar::tabRemoved(int index) {
    QnWorkbenchLayout *layout = m_layouts[index];
    disconnect(layout, NULL, this, NULL);
    m_layouts.removeAt(index);

    checkInvariants();

    updateTabsClosable();
    updateCurrentLayout();

    emit layoutRemoved(layout);
}

void QnLayoutTabBar::at_tabMoved(int from, int to) {
    m_layouts.move(from, to);
}

void QnLayoutTabBar::at_tabCloseRequested(int index) {
    if (count() <= 1)
        return; /* Don't remove the last tab. */

    removeTab(index);
}

void QnLayoutTabBar::at_currentChanged(int index) {
    if(m_layouts.size() != count())
        return; /* Was called from add/remove function before our handler was able to sync, current layout will be updated later. */

    updateCurrentLayout();
}

void QnLayoutTabBar::at_layout_aboutToBeDestroyed() {
    removeLayout(static_cast<QnWorkbenchLayout *>(sender()));
}