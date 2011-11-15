#include "display_state.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <ui/model/layout_model.h>
#include <ui/model/layout_grid_mapper.h>
#include <ui/model/resource_item_model.h>

QnDisplayState::QnDisplayState(QObject *parent):
    QObject(parent),
    m_layout(NULL),
    m_mapper(new QnLayoutGridMapper(this)),
    m_selectedItem(NULL),
    m_zoomedItem(NULL)
{}    

QnDisplayState::~QnDisplayState() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

void QnDisplayState::setLayout(QnLayoutModel *layout) {
    if(layout == m_layout)
        return;

    QnLayoutModel *oldLayout = m_layout;

    if(m_layout != NULL) {
        setSelectedItem(NULL);
        setZoomedItem(NULL);

        disconnect(m_layout, NULL, this, NULL);
    }

    m_layout = layout;

    if(m_layout != NULL) {
        connect(m_layout, SIGNAL(itemAboutToBeRemoved(QnLayoutItemModel *)),    this, SLOT(at_item_aboutToBeRemoved(QnLayoutItemModel *)));
        connect(m_layout, SIGNAL(aboutToBeDestroyed()),                         this, SLOT(at_layout_aboutToBeDestroyed()));
    }

    emit layoutChanged(oldLayout, m_layout);
}

void QnDisplayState::setMode(Mode mode) {
    if(m_mode == mode)
        return;

    m_mode = mode;

    emit modeChanged();
}

void QnDisplayState::setSelectedItem(QnLayoutItemModel *item) {
    if(m_selectedItem == item)
        return;

    if(item != NULL && item->layout() != m_layout) {
        qnWarning("Cannot select an item from another layout.");
        return;
    }

    QnLayoutItemModel *oldSelectedItem = m_selectedItem;
    m_selectedItem = item;
    
    emit selectedItemChanged(oldSelectedItem, m_selectedItem);
}

void QnDisplayState::setZoomedItem(QnLayoutItemModel *item) {
    if(m_zoomedItem == item)
        return;

    if(item != NULL && item->layout() != m_layout) {
        qnWarning("Cannot zoom to an item from another layout.");
        return;
    }

    QnLayoutItemModel *oldZoomedItem = m_zoomedItem;
    m_zoomedItem = item;

    emit zoomedItemChanged(oldZoomedItem, m_zoomedItem);
}

void QnDisplayState::at_item_aboutToBeRemoved(QnLayoutItemModel *item) {
    if(item == m_selectedItem)
        setSelectedItem(NULL);

    if(item == m_zoomedItem)
        setZoomedItem(NULL);
}

void QnDisplayState::at_layout_aboutToBeDestroyed() {
    setLayout(NULL);
}
