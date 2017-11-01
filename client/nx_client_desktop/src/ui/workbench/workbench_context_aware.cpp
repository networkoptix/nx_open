#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtWidgets/QGraphicsItem>
#include <QtWidgets/QGraphicsScene>
#include <QtQml/QtQml>

#include <utils/common/warnings.h>
#include <ui/widgets/main_window.h>

#include "workbench_context.h"

using namespace nx::client::desktop::ui;

QString QnWorkbenchContextAware::kQmlContextPropertyName(lit("workbenchContext"));

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject* parent, InitializationMode initMode):
    m_parent(parent),
    m_initializationMode(initMode)
{
    if (m_initializationMode == InitializationMode::instant)
        init(parent);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext* context)
{
    init(context);
}

QnWorkbenchContextAware::~QnWorkbenchContextAware()
{
}

QnWorkbenchContext* QnWorkbenchContextAware::context() const
{
    if (!m_context && m_parent && m_initializationMode != InitializationMode::instant)
    {
        const auto mutableThis = const_cast<QnWorkbenchContextAware*>(this);

        if (m_initializationMode == InitializationMode::lazy)
            mutableThis->init(m_parent);
        else if (m_initializationMode == InitializationMode::qmlContext)
            mutableThis->initFromQmlContext();
    }

    NX_ASSERT(m_context, Q_FUNC_INFO, "Initialization failed.");

    return m_context;
}

bool QnWorkbenchContextAware::isContextInitialized() const
{
    return m_context != nullptr;
}

void QnWorkbenchContextAware::initializeContext()
{
    NX_ASSERT(m_initializationMode == InitializationMode::manual);
    NX_ASSERT(!m_context, Q_FUNC_INFO, "Double initialization.");
    NX_ASSERT(m_parent);
    init(m_parent);
}

void QnWorkbenchContextAware::init(QObject* parent)
{
    while (parent)
    {
        if (const auto contextAware = dynamic_cast<QnWorkbenchContextAware*>(parent))
        {
            if (contextAware->isContextInitialized())
            {
                m_context = contextAware->context();
                afterContextInitialized();
            }
            return;
        }

        if (const auto context = dynamic_cast<QnWorkbenchContext*>(parent))
        {
            m_context = context;
            afterContextInitialized();
            return;
        }

        if (parent->parent())
        {
            parent = parent->parent();
        }
        else
        {
            if (const auto parentItem = qobject_cast<QGraphicsItem*>(parent))
                parent = parentItem->scene();
            else if (const auto parentWidget = qobject_cast<QWidget*>(parent))
                parent = parentWidget->parentWidget();
            else
                parent = nullptr;
        }
    }

    NX_ASSERT(parent, Q_FUNC_INFO,
        "Invalid parent. Use lazy initialization if you want to make ui widgets context-aware.");
}

void QnWorkbenchContextAware::init(QnWorkbenchContext* context)
{
    NX_ASSERT(context, Q_FUNC_INFO, "Invalid context.");
    NX_ASSERT(!m_context, Q_FUNC_INFO, "Double context initialization.");
    m_context = context;
    afterContextInitialized();
}

void QnWorkbenchContextAware::initFromQmlContext()
{
    if (m_context)
        return;

    NX_ASSERT(m_initializationMode == InitializationMode::qmlContext);
    if (m_initializationMode != InitializationMode::qmlContext)
        return;

    NX_ASSERT(m_parent, Q_FUNC_INFO,
        "QML context based initialization requires parent to be set.");

    const auto qmlContext = ::qmlContext(m_parent);
    if (!qmlContext)
        return;

    const auto workbenchContext =
        qmlContext->contextProperty(kQmlContextPropertyName).value<QnWorkbenchContext*>();

    if (workbenchContext)
        init(workbenchContext);
}

void QnWorkbenchContextAware::afterContextInitialized()
{
}

QAction* QnWorkbenchContextAware::action(const action::IDType id) const
{
    return context()->action(id);
}

action::Manager* QnWorkbenchContextAware::menu() const
{
    return context()->menu();
}

QnWorkbench* QnWorkbenchContextAware::workbench() const
{
    return context()->workbench();
}

QnWorkbenchLayoutSnapshotManager* QnWorkbenchContextAware::snapshotManager() const
{
    return context()->snapshotManager();
}

QnWorkbenchAccessController* QnWorkbenchContextAware::accessController() const
{
    return context()->accessController();
}

QnWorkbenchDisplay* QnWorkbenchContextAware::display() const
{
    return context()->display();
}

QnWorkbenchNavigator* QnWorkbenchContextAware::navigator() const
{
    return context()->navigator();
}

MainWindow* QnWorkbenchContextAware::mainWindow() const
{
    return context()->mainWindow();
}

QWidget* QnWorkbenchContextAware::mainWindowWidget() const
{
    return mainWindow();
}
