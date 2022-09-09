// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/watermark/watermark.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>

class QnWorkbenchContext;
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchCustomizer;

namespace nx::vms::client::desktop {

class MainWindow;
class SystemContext;
class Workbench;

} // namespace nx::vms::client::desktop

/**
 * This class simplifies access to workbench context.
 *
 * If some class needs access to workbench context, it can simply derive from
 * QnWorkbenchContextAware and pass its context-aware parent into constructor.
 */
class QnWorkbenchContextAware: public nx::vms::client::desktop::SystemContextAware
{
public:
    static QString kQmlWorkbenchContextPropertyName;

    enum class InitializationMode
    {
        instant, /**< Initialize context from parent in constructor. */
        lazy, /**< Initialize context from parent when context() is called first time. */
        manual, /**< Initialize context later with manual call initializeContext(). */
        qmlContext, /**< Initialize context later using QML context property. */
    };

    /**
     * Constructor.
     *
     * @param parent Parent for this object. Must itself be a child of some context-aware object,
     *     or of a context. Must not be null. If lazyInitialization is true this can also be an
     *     object itself. In this case context initialization will be performed when the first
     *     getter is called. The context will be found in QML context property.
     */
    QnWorkbenchContextAware(
        QObject* parent,
        InitializationMode initMode = InitializationMode::instant);

    QnWorkbenchContextAware(QnWorkbenchContext* context);

    /**
     * Virtual destructor.
     *
     * We do dynamic_casts to QnWorkbenchContextAware, so this class better have a vtable.
     */
    virtual ~QnWorkbenchContextAware();

    /**
     * @returns Context associated with this context-aware object.
     */
    QnWorkbenchContext* context() const;

    /** Check if context is initialized. */
    bool isContextInitialized() const;

    /** Initialize context manually. */
    void initializeContext();

    nx::vms::client::desktop::SystemContext* systemContext() const;

protected:
    QAction* action(const nx::vms::client::desktop::ui::action::IDType id) const;

    nx::vms::client::desktop::ui::action::Manager* menu() const;

    nx::vms::client::desktop::Workbench* workbench() const;

    QnWorkbenchAccessController* accessController() const;

    QnWorkbenchDisplay* display() const;

    QnWorkbenchNavigator* navigator() const;

    nx::vms::client::desktop::MainWindow* mainWindow() const;

    /**
     * @return The same as mainWindow() but casted to QWidget*, so caller don't need to include
     * MainWindow header.
     */
    QWidget* mainWindowWidget() const;

    nx::core::Watermark watermark() const;

private:
    void init(QObject* parent);
    void init(QnWorkbenchContext* context);
    void initFromQmlContext();

private:
    QObject* m_parent = nullptr;
    InitializationMode m_initializationMode = InitializationMode::instant;
    QnWorkbenchContext* m_context = nullptr;
};
