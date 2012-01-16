#include "workbench.h"

#include <utils/common/warnings.h>
#include "workbench_layout.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"

QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    m_mode(VIEWING),
    m_layout(NULL)
{
    qRegisterMetaType<QnWorkbench::ItemRole>("QnWorkbench::ItemRole");

    m_itemByRole[RAISED] = m_itemByRole[ZOOMED] = 0;

    m_mapper = new QnWorkbenchGridMapper(this);

    m_dummyLayout = new QnWorkbenchLayout(this);
    setLayout(m_dummyLayout);
}

QnWorkbench::~QnWorkbench() {
    m_dummyLayout = NULL;
    clear();

    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

void QnWorkbench::clear() {
    setMode(VIEWING);
    setLayout(NULL);
}

void QnWorkbench::setLayout(QnWorkbenchLayout *layout) {
    if(m_layout == layout)
        return;

    QRect oldBoundingRect;
    QRect newBoundingRect;

    Q_EMIT layoutAboutToBeChanged();
    /* Clean up old layout.
     * It may be NULL only when this function is called from constructor. */
    if(m_layout != NULL) {
        oldBoundingRect = m_layout->boundingRect();

        foreach(QnWorkbenchItem *item, m_layout->items())
            at_layout_itemRemoved(item);

        disconnect(m_layout, NULL, this, NULL);
    }

    /* Prepare new layout. */
    m_layout = layout;
    if(m_layout == NULL && m_dummyLayout != NULL) {
        m_dummyLayout->clear();
        m_layout = m_dummyLayout;
    }
    Q_EMIT layoutChanged();

    /* Set up new layout. */
    if(m_layout != NULL) {
        connect(m_layout, SIGNAL(itemAdded(QnWorkbenchItem *)),             this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
        connect(m_layout, SIGNAL(itemRemoved(QnWorkbenchItem *)),           this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
        connect(m_layout, SIGNAL(aboutToBeDestroyed()),                     this, SLOT(at_layout_aboutToBeDestroyed()));
        connect(m_layout, SIGNAL(boundingRectChanged()),                    this, SIGNAL(boundingRectChanged()));

        foreach(QnWorkbenchItem *item, m_layout->items())
            at_layout_itemAdded(item);

        newBoundingRect = m_layout->boundingRect();
    }

    if (newBoundingRect != oldBoundingRect)
        emit boundingRectChanged();
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

    if(item != NULL && item->layout() != m_layout) {
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
    setLayout(NULL);
}
