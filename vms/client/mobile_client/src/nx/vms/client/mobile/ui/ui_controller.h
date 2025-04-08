// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/mobile/window_context_aware.h>

Q_MOC_INCLUDE("nx/vms/client/mobile/ui/detail/measurements.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/ui/detail/screens.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/ui/detail/window_helpers.h")

namespace nx::vms::client::mobile {

namespace detail {

class Measurements;
class Screens;
class WindowHelpers;

} // namespace detail

class UiController: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = WindowContextAware;

    Q_PROPERTY(detail::Measurements* measurements
        READ measurements
        CONSTANT)

    Q_PROPERTY(detail::WindowHelpers* windowHelpers
        READ windowHelpers
        CONSTANT)

    Q_PROPERTY(detail::Screens* screens
        READ screens
        CONSTANT)

public:
    static void registerQmlType();

    UiController(WindowContext* context,
        QObject* parent = nullptr);
    virtual ~UiController() override;

    detail::Measurements* measurements() const;

    detail::WindowHelpers* windowHelpers() const;

    detail::Screens* screens() const;

    // Invokables

    /** Check saved properties and try to restore last used connection. */
    Q_INVOKABLE bool tryRestoreLastUsedConnection();

    Q_INVOKABLE void showMessage(const QString& title, const QString& message) const;

    Q_INVOKABLE void showConnectionErrorMessage(
        const QString& systemName,
        const QString& errorText);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
