// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRect>

#include <nx/utils/impl_ptr.h>

#include "client_state_handler.h"
#include "window_geometry_state.h"

namespace nx::vms::client::desktop {

class WindowControlInterface
{
public:
    virtual ~WindowControlInterface() {}

    virtual QRect windowRect() const = 0;
    virtual void setWindowRect(const QRect& rect) = 0;

    virtual Qt::WindowStates windowState() const = 0;
    virtual void setWindowState(Qt::WindowStates state) = 0;

    virtual int nextFreeScreen() const = 0;
    virtual QList<QRect> suitableSurface() const = 0;
    virtual int windowScreen() const = 0;


// TODO: coordinates: logical, physical
// TODO: is fullscreen? (+ macos) - maximize
// TODO: screen number
};
using WindowControlInterfacePtr = std::unique_ptr<WindowControlInterface>;

class NX_VMS_CLIENT_DESKTOP_API WindowGeometryManager:
    public QObject,
    public ClientStateDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    static const QString kWindowGeometryData;

    /**
     * Instantiate the geometry manager using the given interface implementation.
     */
    explicit WindowGeometryManager(WindowControlInterfacePtr control, QObject* parent = nullptr);
    virtual ~WindowGeometryManager() override;

    bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& params) override;
    void saveState(DelegateState* state, SubstateFlags flags) override;

    void createInheritedState(
        DelegateState* state,
        SubstateFlags flags,
        const QStringList& resources) override;

    DelegateState defaultState() const override;

    /**
     *  Get window geometry and state of the current client instance window.
     */
    WindowGeometryState windowGeometry() const;

    /**
     * Set window geometry and state.
     * May partially fail if the given geometry is not valid (i.e. the window can't be placed into
     * the provided position).
     */
    void setWindowGeometry(const WindowGeometryState& value);

    /**
     * Check if the given window geometry is compatitible with the current system screen configuration.
     */
    bool isValid(const WindowGeometryState& value) const;

private:
    WindowGeometryState calculateDefaultGeometry() const;
    void fixWindowGeometryIfNeeded(WindowGeometryState* state) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
