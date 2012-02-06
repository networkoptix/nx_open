#include "workbench.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "workbench_layout.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"

QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    m_mode(VIEWING),
    m_currentLayout(NULL)
{
    qRegisterMetaType<QnWorkbench::ItemRole>("QnWorkbench::ItemRole");

    m_itemByRole[RAISED] = m_itemByRole[ZOOMED] = 0;

    m_mapper = new QnWorkbenchGridMapper(this);

    m_dummyLayout = new QnWorkbenchLayout(this);
    setCurrentLayout(m_dummyLayout);
}

QnWorkbench::~QnWorkbench() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    m_dummyLayout = NULL;
    clear();
}

void QnWorkbench::clear() {
    setMode(VIEWING);
    setCurrentLayout(NULL);
    
    while(!m_layouts.empty())
        removeLayout(m_layouts.back());
}

void QnWorkbench::addLayout(QnWorkbenchLayout *layout) {
    insertLayout(layout, m_layouts.size());
}

void QnWorkbench::insertLayout(QnWorkbenchLayout *layout, int index) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(m_layouts.contains(layout)) {
        qnWarning("Given layout is already in this workbench's layouts list.");
        return;
    }

    /* Silently fix index. */
    index = qBound(0, index, m_layouts.size());

    m_layouts.insert(index, layout);
    connect(layout, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_layout_aboutToBeDestroyed()));

    emit layoutsChanged();
}

void QnWorkbench::removeLayout(QnWorkbenchLayout *layout) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(!m_layouts.contains(layout))
        return; /* Removing a layout that is not there is OK. */

    /* Update current layout if it's being removed. */
    if(layout == m_currentLayout) {
        QnWorkbenchLayout *newCurrentLayout = NULL;
        
        int newCurrentIndex = m_layouts.indexOf(m_currentLayout);
        newCurrentIndex++;
        if(newCurrentIndex >= m_layouts.size())
            newCurrentIndex -= 2;
        if(newCurrentIndex >= 0)
            newCurrentLayout = m_layouts[newCurrentIndex];

        setCurrentLayout(newCurrentLayout);
    }

    m_layouts.removeOne(layout);
    disconnect(layout, NULL, this, NULL);

    emit layoutsChanged();
}

void QnWorkbench::moveLayout(QnWorkbenchLayout *layout, int index) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(!m_layouts.contains(layout)) {
        qnWarning("Given layout is not in this workbench's layouts list.");
        return;
    }

    /* Silently fix index. */
    index = qBound(0, index, m_layouts.size());

    int currentIndex = m_layouts.indexOf(layout);
    if(currentIndex == index)
        return;

    m_layouts.move(currentIndex, index);

    emit layoutsChanged();
}

int QnWorkbench::layoutIndex(QnWorkbenchLayout *layout) {
    return m_layouts.indexOf(layout);
}

void QnWorkbench::setCurrentLayout(QnWorkbenchLayout *layout) {
    if(m_currentLayout == layout)
        return;

    if(!m_layouts.contains(layout) && layout != m_dummyLayout)
        addLayout(layout);

    QRect oldBoundingRect, newBoundingRect;

    emit currentLayoutAboutToBeChanged();
    /* Clean up old layout.
     * It may be NULL only when this function is called from constructor. */
    if(m_currentLayout != NULL) {
        oldBoundingRect = m_currentLayout->boundingRect();

        foreach(QnWorkbenchItem *item, m_currentLayout->items())
            at_layout_itemRemoved(item);

        disconnect(m_currentLayout, SIGNAL(itemAdded(QnWorkbenchItem *)),             this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
        disconnect(m_currentLayout, SIGNAL(itemRemoved(QnWorkbenchItem *)),           this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
        disconnect(m_currentLayout, SIGNAL(boundingRectChanged()),                    this, SIGNAL(boundingRectChanged()));
    }

    /* Prepare new layout. */
    m_currentLayout = layout;
    if(m_currentLayout == NULL && m_dummyLayout != NULL) {
        m_dummyLayout->clear();
        m_currentLayout = m_dummyLayout;
    }
    
    /* Set up new layout. 
     * 
     * The fact that new layout is NULL means that we're in destructor, so no
     * signals should be emitted. */
    if(m_currentLayout != NULL) {
        emit currentLayoutChanged();

        connect(m_currentLayout, SIGNAL(itemAdded(QnWorkbenchItem *)),             this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
        connect(m_currentLayout, SIGNAL(itemRemoved(QnWorkbenchItem *)),           this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
        connect(m_currentLayout, SIGNAL(boundingRectChanged()),                    this, SIGNAL(boundingRectChanged()));

        foreach(QnWorkbenchItem *item, m_currentLayout->items())
            at_layout_itemAdded(item);

        newBoundingRect = m_currentLayout->boundingRect();

        if (newBoundingRect != oldBoundingRect)
            emit boundingRectChanged();
    }
}

void QnWorkbench::setMode(Mode mode) {
    if(m_mode == mode)
        return;

    m_mode = mode;

    emit modeChanged();
}

QnWorkbenchItem *QnWorkbench::item(ItemRole role) {
    Q_ASSERT(role >= 0 && role < ITEM_ROLE_COUNT);

    return m_itemByRole[role];
}

void QnWorkbench::setItem(ItemRole role, QnWorkbenchItem *item) {
    Q_ASSERT(role >= 0 && role < ITEM_ROLE_COUNT);

    if(m_itemByRole[role] == item)
        return;

    if(item != NULL && item->layout() != m_currentLayout) {
        qnWarning("Cannot set a role for an item from another layout.");
        return;
    }

    m_itemByRole[role] = item;

    emit itemChanged(role);
}

void QnWorkbench::at_layout_itemAdded(QnWorkbenchItem *item) {
    emit itemAdded(item);
}

void QnWorkbench::at_layout_itemRemoved(QnWorkbenchItem *item) {
    for(int i = 0; i < ITEM_ROLE_COUNT; i++)
        if(item == m_itemByRole[i])
            setItem(static_cast<ItemRole>(i), NULL);

    emit itemRemoved(item);
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    removeLayout(checked_cast<QnWorkbenchLayout *>(sender()));
}
