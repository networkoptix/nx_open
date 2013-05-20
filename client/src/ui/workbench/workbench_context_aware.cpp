#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtGui/QGraphicsItem>
#include <QtGui/QGraphicsScene>

#include <utils/common/warnings.h>
#include <core/resource_managment/resource_pool.h>

#include "workbench_context.h"

Q_GLOBAL_STATIC_WITH_ARGS(QnWorkbenchContext, qn_staticContext, (qnResPool))

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent) {
    init(parent);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext *context) {
    init(context);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent, QnWorkbenchContext *context) {
    if(context) {
        init(context);
    } else if(parent) {
        init(parent);
    } else {
        qnNullCritical(parent);
        m_context = qn_staticContext();
    }
}

void QnWorkbenchContextAware::init(QObject *parent) {
    while(true) {
        if(parent == NULL) {
            qnCritical("No context-aware parent found.");
            m_context = qn_staticContext();
            return;
        }

        QnWorkbenchContextAware *contextAware = dynamic_cast<QnWorkbenchContextAware *>(parent);
        if(contextAware != NULL) {
            m_context = contextAware->context();
            return;
        }

        QnWorkbenchContext *context = dynamic_cast<QnWorkbenchContext *>(parent);
        if(context != NULL) {
            m_context = context;
            return;
        }

        if(parent->parent()) {
            parent = parent->parent();
        } else {
            if(QGraphicsItem *parentItem = dynamic_cast<QGraphicsItem *>(parent)) {
                parent = parentItem->scene();
            } else {
                parent = NULL;
            }
        }
    }
}

void QnWorkbenchContextAware::init(QnWorkbenchContext *context) {
    m_context = context;

    if(!context) {
        qnNullCritical(context);
        m_context = qn_staticContext();
        return;
    }
}

QAction *QnWorkbenchContextAware::action(const Qn::ActionId id) const {
    return context()->action(id);
}

QnActionManager *QnWorkbenchContextAware::menu() const {
    return context()->menu();
}

QnWorkbench *QnWorkbenchContextAware::workbench() const {
    return context()->workbench();
}

QnResourcePool *QnWorkbenchContextAware::resourcePool() const {
    return context()->resourcePool();
}

QnWorkbenchSynchronizer *QnWorkbenchContextAware::synchronizer() const {
    return context()->synchronizer();
}

QnWorkbenchLayoutSnapshotManager *QnWorkbenchContextAware::snapshotManager() const {
    return context()->snapshotManager();
}

QnWorkbenchAccessController *QnWorkbenchContextAware::accessController() const {
    return context()->accessController();
}

QnWorkbenchDisplay *QnWorkbenchContextAware::display() const {
    return context()->display();
}

QnWorkbenchNavigator *QnWorkbenchContextAware::navigator() const {
    return context()->navigator();
}


