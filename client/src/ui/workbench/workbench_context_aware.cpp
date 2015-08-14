#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>

#include <utils/common/warnings.h>
#include <core/resource_management/resource_pool.h>

#include "workbench_context.h"

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent, bool lazyInitialization)
    : m_context(nullptr)
    , m_initialized(false)
{
    init(parent, lazyInitialization);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext *context)
    : m_context(nullptr)
    , m_initialized(false)
{
    init(context);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent, QnWorkbenchContext *context)
    : m_context(nullptr)
    , m_initialized(false)
{
    if(context) {
        init(context);
    } else {
        init(parent);
    }
}

void QnWorkbenchContextAware::initializeContext(QObject *parent) {
    Q_ASSERT_X(!m_initialized, Q_FUNC_INFO, "Double initialization");
    init(parent);
}

void QnWorkbenchContextAware::initializeContext(QnWorkbenchContext *context) {
    Q_ASSERT_X(!m_initialized, Q_FUNC_INFO, "Double initialization");
    init(context);
}

void QnWorkbenchContextAware::init(QObject *parent, bool lazyInitialization) {
    while(true) {
        if (!lazyInitialization)
            Q_ASSERT_X(parent, Q_FUNC_INFO, "Invalid parent. Use lazy initialization if you want to make ui widgets context-aware.");
        if (!parent && lazyInitialization)
            return;

        QnWorkbenchContextAware *contextAware = dynamic_cast<QnWorkbenchContextAware *>(parent);
        if(contextAware != NULL) {
            m_context = contextAware->context();
            Q_ASSERT_X(m_context, Q_FUNC_INFO, "Invalid context");
            m_initialized = true;
            afterContextInitialized();
            return;
        }

        QnWorkbenchContext *context = dynamic_cast<QnWorkbenchContext *>(parent);
        if(context != NULL) {
            m_context = context;
            m_initialized = true;
            afterContextInitialized();
            return;
        }

        if(parent->parent()) {
            parent = parent->parent();
        } else {
            if(QGraphicsItem *parentItem = dynamic_cast<QGraphicsItem *>(parent)) {
                parent = parentItem->scene();
            } else if (QWidget* parentWidget = dynamic_cast<QWidget*>(parent)) {
                parent = parentWidget->parentWidget();
            }            
            else {
                parent = NULL;
            }
        }
    }
}

void QnWorkbenchContextAware::init(QnWorkbenchContext *context) {
    Q_ASSERT_X(!m_context, Q_FUNC_INFO, "Invalid context");
    m_context = context;
    m_initialized = true;
    afterContextInitialized();
}

void QnWorkbenchContextAware::afterContextInitialized() {
    //do nothing
}

QAction *QnWorkbenchContextAware::action(const Qn::ActionId id) const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->action(id);
}

QnActionManager *QnWorkbenchContextAware::menu() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->menu();
}

QnWorkbench *QnWorkbenchContextAware::workbench() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->workbench();
}

QnResourcePool *QnWorkbenchContextAware::resourcePool() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->resourcePool();
}

QnWorkbenchSynchronizer *QnWorkbenchContextAware::synchronizer() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->synchronizer();
}

QnWorkbenchLayoutSnapshotManager *QnWorkbenchContextAware::snapshotManager() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->snapshotManager();
}

QnWorkbenchAccessController *QnWorkbenchContextAware::accessController() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->accessController();
}

QnWorkbenchDisplay *QnWorkbenchContextAware::display() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->display();
}

QnWorkbenchNavigator *QnWorkbenchContextAware::navigator() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->navigator();
}

QWidget *QnWorkbenchContextAware::mainWindow() const {
    Q_ASSERT_X(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->mainWindow();
}
