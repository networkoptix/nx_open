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
 
v oid QnWorkbenchContextAware::init(QObject *parent) {
     while(true) {
         Q_ASSERT(parent);
		  
        Qn WorkbenchContextAware *contextAware = dynamic_cast<QnWorkbenchContextAware *>(parent);
        if( contextAware != NULL) {
             m_context = contextAware->context();
            r eturn;
        } 
		 
        Q nWorkbenchContext *context = dynamic_cast<QnWorkbenchContext *>(parent);
        if (context != NULL) {
             m_context = context;
             return;
        } 
		 
        i f(parent->parent()) {
             parent = parent->parent();
        } e lse {
             if(QGraphicsItem *parentItem = dynamic_cast<QGraphicsItem *>(parent)) {
                 parent = parentItem->scene();
            }  else {
                 parent = NULL;
            } 
        } 
    } 
} 
 
v oid QnWorkbenchContextAware::init(QnWorkbenchContext *context) {
     Q_ASSERT(context);
     m_context = context;
} 
 
Q Action *QnWorkbenchContextAware::action(const Qn::ActionId id) const {
     return context()->action(id);
} 
 
Q nActionManager *QnWorkbenchContextAware::menu() const {
     return context()->menu();
} 
 
Q nWorkbench *QnWorkbenchContextAware::workbench() const {
     return context()->workbench();
} 
 
Q nResourcePool *QnWorkbenchContextAware::resourcePool() const {
     return context()->resourcePool();
} 
 
Q nWorkbenchSynchronizer *QnWorkbenchContextAware::synchronizer() const {
     return context()->synchronizer();
} 
 
Q nWorkbenchLayoutSnapshotManager *QnWorkbenchContextAware::snapshotManager() const {
     return context()->snapshotManager();
} 
 
Q nWorkbenchAccessController *QnWorkbenchContextAware::accessController() const {
     return context()->accessController();
} 
 
Q nWorkbenchDisplay *QnWorkbenchContextAware::display() const {
     return context()->display();
} 
 
Q nWorkbenchNavigator *QnWorkbenchContextAware::navigator() const {
     return context()->navigator();
} 
 
Q Widget *QnWorkbenchContextAware::mainWindow() const {
     return context()->mainWindow();
} 
                                                   