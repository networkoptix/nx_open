#include "display_state.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <ui/model/layout_model.h>
#include <ui/model/layout_grid_mapper.h>
#include <ui/model/resource_item_model.h>

QnDisplayState::QnDisplayState(QnLayoutModel *model, QObject *parent):
    QObject(parent),
    m_model(model),
    m_mapper(new QnLayoutGridMapper(this)),
    m_selectedItem(NULL),
    m_zoomedItem(NULL)
{
    assert(model != NULL);

    connect(model, SIGNAL(itemAboutToBeRemoved(QnLayoutItemModel *)), this, SLOT(at_item_aboutToBeRemoved(QnLayoutItemModel *)));
}    

QnDisplayState::~QnDisplayState() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
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

    if(item != NULL && item->layout() != m_model) {
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

    if(item != NULL && item->layout() != m_model) {
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