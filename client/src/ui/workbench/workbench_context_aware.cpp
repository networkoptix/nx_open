#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>

#include <utils/common/warnings.h>
#include <core/resource_management/resource_pool.h>

#include "workbench_context.h"

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent) {
    init(parent);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext *context) {
    init(context);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent, QnWorkbenchContext *context) {
    if(context) {
        init(context);
    } else {
        Q_ASSERT(parent);
        init(parent);
    }
}

void QnWorkbenchContextAware::init(QObject *parent) {
    while(true) {
        Q_ASSERT(parent);

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
    Q_ASSERT(context);
    m_context = context;
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

QWidget *QnWorkbenchContextAware::mainWindow() const {
    return context()->mainWindow();
}
