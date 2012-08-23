#include "workbench_context_aware.h"
#include <QObject>
#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_pool.h>
#include <ui/actions/action_manager.h>
#include "workbench_context.h"

Q_GLOBAL_STATIC_WITH_ARGS(QnWorkbenchContext, qn_staticContext, (qnResPool))

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent) {
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

        parent = parent->parent();
    }
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext *context): m_context(context) {
    if(!context) {
        qnNullCritical(context);
        m_context = qn_staticContext();
        return;
    }
}

QAction *QnWorkbenchContextAware::action(const Qn::ActionId id) const {
    return menu()->action(id);
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

QnVideoServerStatisticsManager *QnWorkbenchContextAware::videoServerStatisticsManager() const{
    return context()->videoServerStatisticsManager();
}

