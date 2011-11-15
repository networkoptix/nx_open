#include "workbench.h"
#include <cassert>
#include <utils/common/warnings.h>
#include "workbench_layout.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"

QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_mapper(new QnWorkbenchGridMapper(this)),
    m_selectedItem(NULL),
    m_zoomedItem(NULL)
{}    

QnWorkbench::~QnWorkbench() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

void QnWorkbench::setLayout(QnWorkbenchLayout *layout) {
    if(layout == m_layout)
        return;

    QnWorkbenchLayout *oldLayout = m_layout;

    if(m_layout != NULL) {
        setSelectedItem(NULL);
        setZoomedItem(NULL);

        disconnect(m_layout, NULL, this, NULL);
    }

    m_layout = layout;

    if(m_layout != NULL) {
        connect(m_layout, SIGNAL(itemAboutToBeRemoved(QnWorkbenchItem *)),    this, SLOT(at_item_aboutToBeRemoved(QnWorkbenchItem *)));
        connect(m_layout, SIGNAL(aboutToBeDestroyed()),                         this, SLOT(at_layout_aboutToBeDestroyed()));
    }

    emit layoutChanged(oldLayout, m_layout);
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

    QnWorkbenchItem *oldSelectedItem = m_selectedItem;
    m_selectedItem = item;
    
    emit selectedItemChanged(oldSelectedItem, m_selectedItem);
}

void QnWorkbench::setZoomedItem(QnWorkbenchItem *item) {
    if(m_zoomedItem == item)
        return;

    if(item != NULL && item->layout() != m_layout) {
        qnWarning("Cannot zoom to an item from another layout.");
        return;
    }

    QnWorkbenchItem *oldZoomedItem = m_zoomedItem;
    m_zoomedItem = item;

    emit zoomedItemChanged(oldZoomedItem, m_zoomedItem);
}

void QnWorkbench::at_item_aboutToBeRemoved(QnWorkbenchItem *item) {
    if(item == m_selectedItem)
        setSelectedItem(NULL);

    if(item == m_zoomedItem)
        setZoomedItem(NULL);
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    setLayout(NULL);
}
