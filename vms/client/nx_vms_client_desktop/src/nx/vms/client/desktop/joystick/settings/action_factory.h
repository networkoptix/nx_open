// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>

#include "device.h"

namespace nx::vms::client::desktop {
namespace joystick {

struct JoystickDescriptor;

class ActionFactory:
    public QObject,
    public core::CommonModuleAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    static QString kLayoutLogicalIdParameterName;
    static QString kLayoutUuidParameterName;
    static QString kHotkeyParameterName;
    static QString kPtzPresetParameterName;

public:
    /**
     * Constructor for supported joystick models.
     */
    ActionFactory(
        const JoystickDescriptor& config,
        QObject* parent = 0);

    virtual ~ActionFactory();

    QString modelName() const;

    void updateConfig(const JoystickDescriptor& config);

    void handleStateChanged(const Device::StickPosition& stick, const Device::ButtonStates& buttons);

signals:
    void actionReady(
        nx::vms::client::desktop::menu::IDType id,
        const nx::vms::client::desktop::menu::Parameters& parameters);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace joystick
} // namespace nx::vms::client::desktop
