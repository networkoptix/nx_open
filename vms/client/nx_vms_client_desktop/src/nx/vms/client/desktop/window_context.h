// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QWidget;
class QQuickWindow;
class QnWorkbenchContext;
class QnWorkbenchRenderWatcher;
class QnWorkbenchStreamSynchronizer;

namespace nx::vms::client::desktop {

class SystemTabBarModel;

class WindowContext: public QObject
{
    Q_OBJECT

public:
    /**
     * Initialize client-level Window Context.
     * Destruction order must be handled by the caller.
     * TODO: @sivanov Invert Workbench Context dependency.
     */
    WindowContext(QnWorkbenchContext* workbenchContext, QObject* parent = nullptr);
    virtual ~WindowContext() override;

    /** Main window. */
    QWidget* mainWindowWidget() const;

    QnWorkbenchContext* workbenchContext() const;

    /**
     * For each Resource Widget on the scene, this class tracks whether it is being currently
     * displayed or not. It provides the necessary getters and signals to track changes of this
     * state.
     */
    QnWorkbenchRenderWatcher* resourceWidgetRenderWatcher() const;

    /**
     * Manages the necessary machinery for synchronized playback of cameras on the scene.
     */
    QnWorkbenchStreamSynchronizer* streamSynchronizer() const;

    SystemTabBarModel* systemTabBarModel() const;

    /**
     * Returns resource browser quick window. Use only for obtaining graphics API information.
     */
    QQuickWindow* quickWindow() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
