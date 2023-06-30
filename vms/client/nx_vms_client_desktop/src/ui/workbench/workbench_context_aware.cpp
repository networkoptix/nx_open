// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_context_aware.h"

#include <QtCore/QObject>
#include <QtQml/QtQml>
#include <QtWidgets/QGraphicsItem>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/graphics/view/graphics_scene.h>
#include <ui/widgets/main_window.h>

#include "workbench_context.h"

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QString QnWorkbenchContextAware::kQmlWorkbenchContextPropertyName("workbenchContext");

QnWorkbenchContextAware::QnWorkbenchContextAware(QObject* parent, InitializationMode initMode):
    // TODO: #GDM initialize from existing context later.
    SystemContextAware(appContext()->currentSystemContext()),
    m_parent(parent),
    m_initializationMode(initMode)
{
    if (m_initializationMode == InitializationMode::instant)
        init(parent);
}

QnWorkbenchContextAware::QnWorkbenchContextAware(QnWorkbenchContext* context):
    // TODO: #GDM initialize from existing context.
    SystemContextAware(appContext()->currentSystemContext())
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

    NX_ASSERT(m_context, "Initialization failed.");

    return m_context;
}

bool QnWorkbenchContextAware::isContextInitialized() const
{
    return m_context != nullptr;
}

void QnWorkbenchContextAware::initializeContext()
{
    NX_ASSERT(m_initializationMode == InitializationMode::manual);
    NX_ASSERT(!m_context, "Double initialization.");
    NX_ASSERT(m_parent);
    init(m_parent);
}

void QnWorkbenchContextAware::init(QObject* parent)
{
    while (parent)
    {
        if (const auto contextAware = dynamic_cast<QnWorkbenchContextAware*>(parent))
        {
            const auto context = contextAware->m_initializationMode == InitializationMode::lazy
                ? contextAware->context()
                : (contextAware->isContextInitialized() ? contextAware->context() : nullptr);

            if (context)
                m_context = context;
            return;
        }

        if (const auto context = dynamic_cast<QnWorkbenchContext*>(parent))
        {
            m_context = context;
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

    NX_ASSERT(parent,
        "Invalid parent. Use lazy initialization if you want to make ui widgets context-aware.");
}

void QnWorkbenchContextAware::init(QnWorkbenchContext* context)
{
    NX_ASSERT(context, "Invalid context.");
    NX_ASSERT(!m_context, "Double context initialization.");
    m_context = context;
}

void QnWorkbenchContextAware::initFromQmlContext()
{
    if (m_context)
        return;

    NX_ASSERT(m_initializationMode == InitializationMode::qmlContext);
    if (m_initializationMode != InitializationMode::qmlContext)
        return;

    NX_ASSERT(m_parent,
        "QML context based initialization requires parent to be set.");

    const auto qmlContext = ::qmlContext(m_parent);
    if (!qmlContext)
        return;

    const auto workbenchContext =
        qmlContext->contextProperty(kQmlWorkbenchContextPropertyName).value<QnWorkbenchContext*>();

    if (workbenchContext)
        init(workbenchContext);
}

QAction* QnWorkbenchContextAware::action(const action::IDType id) const
{
    return context()->action(id);
}

action::Manager* QnWorkbenchContextAware::menu() const
{
    return context()->menu();
}

Workbench* QnWorkbenchContextAware::workbench() const
{
    return context()->workbench();
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

nx::vms::client::desktop::MainWindow* QnWorkbenchContextAware::mainWindow() const
{
    return context()->mainWindow();
}

QWidget* QnWorkbenchContextAware::mainWindowWidget() const
{
    return mainWindow();
}

nx::core::Watermark QnWorkbenchContextAware::watermark() const
{
    return context()->watermark();
}
