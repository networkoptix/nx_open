#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>

#include <utils/common/warnings.h>
#include <ui/widgets/main_window.h>

#include "workbench_context.h"

using namespace nx::client::desktop::ui;

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject* parent, bool lazyInitialization):
    m_context(nullptr),
    m_initialized(false)
{
    init(parent, lazyInitialization);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext *context):
    m_context(nullptr),
    m_initialized(false)
{
    init(context);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject *parent,
    QnWorkbenchContext *context)
    :
    m_context(nullptr),
    m_initialized(false)
{
    init(context ? context : parent);
}

void QnWorkbenchContextAware::initializeContext(QObject *parent)
{
    NX_ASSERT(!m_initialized, Q_FUNC_INFO, "Double initialization");
    init(parent);
}

void QnWorkbenchContextAware::initializeContext(QnWorkbenchContext *context)
{
    NX_ASSERT(!m_initialized, Q_FUNC_INFO, "Double initialization");
    init(context);
}

void QnWorkbenchContextAware::init(QObject *parent, bool lazyInitialization)
{
    while (true)
    {
        if (!lazyInitialization)
            NX_ASSERT(parent, Q_FUNC_INFO, "Invalid parent. Use lazy initialization if you want to make ui widgets context-aware.");
        if (!parent && lazyInitialization)
            return;

        QnWorkbenchContextAware *contextAware = dynamic_cast<QnWorkbenchContextAware *>(parent);
        if (contextAware != NULL)
        {
            m_context = contextAware->context();
            NX_ASSERT(m_context, Q_FUNC_INFO, "Invalid context");
            m_initialized = true;
            afterContextInitialized();
            return;
        }

        QnWorkbenchContext *context = dynamic_cast<QnWorkbenchContext *>(parent);
        if (context != NULL)
        {
            m_context = context;
            m_initialized = true;
            afterContextInitialized();
            return;
        }

        if (parent->parent())
        {
            parent = parent->parent();
        }
        else
        {
            if (QGraphicsItem *parentItem = dynamic_cast<QGraphicsItem *>(parent))
            {
                parent = parentItem->scene();
            }
            else if (QWidget* parentWidget = dynamic_cast<QWidget*>(parent))
            {
                parent = parentWidget->parentWidget();
            }
            else
            {
                parent = NULL;
            }
        }
    }
}

void QnWorkbenchContextAware::init(QnWorkbenchContext *context)
{
    NX_ASSERT(context, Q_FUNC_INFO, "Invalid context");
    NX_ASSERT(!m_context, Q_FUNC_INFO, "Double context initialization");
    m_context = context;
    m_initialized = true;
    afterContextInitialized();
}

void QnWorkbenchContextAware::afterContextInitialized()
{
    //do nothing
}

QAction *QnWorkbenchContextAware::action(const action::IDType id) const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->action(id);
}

action::Manager* QnWorkbenchContextAware::menu() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->menu();
}

QnWorkbench *QnWorkbenchContextAware::workbench() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->workbench();
}

QnWorkbenchLayoutSnapshotManager *QnWorkbenchContextAware::snapshotManager() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->snapshotManager();
}

QnWorkbenchAccessController *QnWorkbenchContextAware::accessController() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->accessController();
}

QnWorkbenchDisplay *QnWorkbenchContextAware::display() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->display();
}

QnWorkbenchNavigator *QnWorkbenchContextAware::navigator() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->navigator();
}

MainWindow* QnWorkbenchContextAware::mainWindow() const
{
    NX_ASSERT(m_initialized, Q_FUNC_INFO, "Initialization failed");
    return context()->mainWindow();
}

QWidget* QnWorkbenchContextAware::mainWindowWidget() const
{
    return mainWindow();
}
