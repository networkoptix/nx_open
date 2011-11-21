#include "workbench.h"
#include <cassert>
#include <utils/common/warnings.h>
#include "workbench_layout.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"

QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    m_mode(VIEWING),
    m_layout(NULL),
    m_dummyLayout(new QnWorkbenchLayout(this)),
    m_mapper(new QnWorkbenchGridMapper(this)),
    m_selectedItem(NULL),
    m_zoomedItem(NULL)
{
    setLayout(m_dummyLayout);
}    

QnWorkbench::~QnWorkbench() {
    clear();

    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    m_dummyLayout = NULL;
}

void QnWorkbench::clear() {
    setMode(VIEWING);
    setLayout(NULL);
}

void QnWorkbench::setLayout(QnWorkbenchLayout *layout) {
    if(m_layout == layout)
        return;

    /* Clean up old layout. 
     * It may be NULL only when this function is called from constructor. */
    if(m_layout != NULL) {
        foreach(QnWorkbenchItem *item, m_layout->items())
            at_layout_itemAboutToBeRemoved(item);

        disconnect(m_layout, NULL, this, NULL);
    }

    /* Prepare new layout. */
    m_layout = layout;
    if(m_layout == NULL && m_dummyLayout != NULL) {
        m_dummyLayout->clear();
        m_layout = m_dummyLayout;
    }
    emit layoutChanged();

    /* Set up new layout. 
     * It may be NULL only when this function is called from destructor. */
    if(m_layout != NULL) {
        connect(m_layout, SIGNAL(itemAdded(QnWorkbenchItem *)),             this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
        connect(m_layout, SIGNAL(itemAboutToBeRemoved(QnWorkbenchItem *)),  this, SLOT(at_layout_itemAboutToBeRemoved(QnWorkbenchItem *)));
        connect(m_layout, SIGNAL(aboutToBeDestroyed()),                     this, SLOT(at_layout_aboutToBeDestroyed()));

        foreach(QnWorkbenchItem *item, m_layout->items())
            at_layout_itemAdded(item);
    }
}

void QnWorkbench::setMode(Mode mode) {
    if(m_mode == mode)
        return;

    m_mode = mode;

    emit modeChanged();
}

void QnWorkbench::setSelectedItem(QnWorkbenchItem *item) {
    if(m_selectedItem == item)
        return;

    if(item != NULL && item->layout() != m_layout) {
        qnWarning("Cannot select an item from another layout.");
        return;
    }

    m_selectedItem = item;
    
    emit selectedItemChanged();
}

void QnWorkbench::setZoomedItem(QnWorkbenchItem *item) {
    if(m_zoomedItem == item)
        return;

    if(item != NULL && item->layout() != m_layout) {
        qnWarning("Cannot zoom to an item from another layout.");
        return;
    }

    m_zoomedItem = item;

    emit zoomedItemChanged();
}

void QnWorkbench::at_layout_itemAdded(QnWorkbenchItem *item) {
    emit itemAdded(item);
}

void QnWorkbench::at_layout_itemAboutToBeRemoved(QnWorkbenchItem *item) {
    if(item == m_selectedItem)
        setSelectedItem(NULL);

    if(item == m_zoomedItem)
        setZoomedItem(NULL);

    emit itemAboutToBeRemoved(item);
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    setLayout(NULL);
}
