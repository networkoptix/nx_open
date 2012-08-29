#include "workbench.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <ui/style/globals.h>
#include "workbench_layout.h"
#include "workbench_grid_mapper.h"
#include "workbench_item.h"

QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    m_currentLayout(NULL)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<Qn::ItemRole>("Qn::ItemRole");
        metaTypesInitialized = true;
    }

    for(int i = 0; i < Qn::ItemRoleCount; i++)
        m_itemByRole[i] = NULL;

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
    setCurrentLayout(NULL);
    
    while(!m_layouts.empty())
        removeLayout(m_layouts.back());
}

QnWorkbenchLayout *QnWorkbench::layout(int index) const {
    if(index < 0 || index >= m_layouts.size()) {
        qnWarning("Invalid layout index '%1'.", index);
        return NULL;
    }

    return m_layouts[index];
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

void QnWorkbench::removeLayout(int index) {
    removeLayout(layout(index));
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

int QnWorkbench::layoutIndex(QnWorkbenchLayout *layout) const {
    return m_layouts.indexOf(layout);
}

void QnWorkbench::setCurrentLayoutIndex(int index) {
    if(m_layouts.empty())
        return;

    setCurrentLayout(m_layouts[qBound(0, index, m_layouts.size())]);
}

void QnWorkbench::setCurrentLayout(QnWorkbenchLayout *layout) {
    if(layout == NULL)
        layout = m_dummyLayout;

    if(m_currentLayout == layout)
        return;

    if(!m_layouts.contains(layout) && layout != m_dummyLayout)
        addLayout(layout);

    qreal oldCellAspectRatio = 0.0, newCellAspectRatio = 0.0;
    QSizeF oldCellSpacing, newCellSpacing;


    emit currentLayoutAboutToBeChanged();
    /* Clean up old layout.
     * It may be NULL only when this function is called from constructor. */
    if(m_currentLayout != NULL) {
        oldCellAspectRatio = m_currentLayout->cellAspectRatio();
        oldCellSpacing = m_currentLayout->cellSpacing();

        foreach(QnWorkbenchItem *item, m_currentLayout->items())
            at_layout_itemRemoved(item);

        disconnect(m_currentLayout, SIGNAL(itemAdded(QnWorkbenchItem *)),           this, NULL);
        disconnect(m_currentLayout, SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, NULL);
        disconnect(m_currentLayout, SIGNAL(cellAspectRatioChanged()),               this, NULL);
        disconnect(m_currentLayout, SIGNAL(cellSpacingChanged()),                   this, NULL);
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

        connect(m_currentLayout,    SIGNAL(itemAdded(QnWorkbenchItem *)),           this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
        connect(m_currentLayout,    SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
        connect(m_currentLayout,    SIGNAL(cellAspectRatioChanged()),               this, SLOT(at_layout_cellAspectRatioChanged()));
        connect(m_currentLayout,    SIGNAL(cellSpacingChanged()),                   this, SLOT(at_layout_cellSpacingChanged()));

        newCellAspectRatio = m_currentLayout->cellAspectRatio();
        newCellSpacing = m_currentLayout->cellSpacing();

        if(!qFuzzyCompare(newCellAspectRatio, oldCellAspectRatio))
            at_layout_cellAspectRatioChanged();

        if(!qFuzzyCompare(newCellSpacing, oldCellSpacing))
            at_layout_cellSpacingChanged();

        foreach(QnWorkbenchItem *item, m_currentLayout->items())
            at_layout_itemAdded(item);
    }
}

QnWorkbenchItem *QnWorkbench::item(Qn::ItemRole role) {
    Q_ASSERT(role >= 0 && role < Qn::ItemRoleCount);

    return m_itemByRole[role];
}

void QnWorkbench::setItem(Qn::ItemRole role, QnWorkbenchItem *item) {
    Q_ASSERT(role >= 0 && role < Qn::ItemRoleCount);

    if(m_itemByRole[role] == item)
        return;

    if(item != NULL && item->layout() != m_currentLayout) {
        qnWarning("Cannot set a role for an item from another layout.");
        return;
    }

    m_itemByRole[role] = item;

    emit itemChanged(role);

    /* Update central item. */
    if(role != Qn::CentralRole) {
        if(m_itemByRole[Qn::ZoomedRole]) {
            setItem(Qn::CentralRole, m_itemByRole[Qn::ZoomedRole]);
        } else if(m_itemByRole[Qn::RaisedRole]) {
            setItem(Qn::CentralRole, m_itemByRole[Qn::RaisedRole]);
        } else if(m_itemByRole[Qn::SingleSelectedRole]) {
            setItem(Qn::CentralRole, m_itemByRole[Qn::SingleSelectedRole]);
        } else if(m_itemByRole[Qn::SingleRole]) {
            setItem(Qn::CentralRole, m_itemByRole[Qn::SingleRole]);
        } else {
            setItem(Qn::CentralRole, NULL);
        }
    }
}

void QnWorkbench::updateSingleRoleItem() {
    if(m_currentLayout->items().size() == 1) {
        setItem(Qn::SingleRole, *m_currentLayout->items().begin());
    } else {
        setItem(Qn::SingleRole, NULL);
    }
}

void QnWorkbench::at_layout_itemAdded(QnWorkbenchItem *item) {
    updateSingleRoleItem();
}

void QnWorkbench::at_layout_itemRemoved(QnWorkbenchItem *item) {
    for(int i = 0; i < Qn::ItemRoleCount; i++)
        if(item == m_itemByRole[i])
            setItem(static_cast<Qn::ItemRole>(i), NULL);

    updateSingleRoleItem();
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    removeLayout(checked_cast<QnWorkbenchLayout *>(sender()));
}

void QnWorkbench::at_layout_cellAspectRatioChanged() {
    qreal unit = qnGlobals->workbenchUnitSize();

    m_mapper->setCellSize(unit, unit / m_currentLayout->cellAspectRatio());
}

void QnWorkbench::at_layout_cellSpacingChanged() {
    qreal unit = qnGlobals->workbenchUnitSize();

    m_mapper->setSpacing(m_currentLayout->cellSpacing() * unit);
}
