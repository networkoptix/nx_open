// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <common/common_module_aware.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>

namespace nx::vms::client::desktop {
namespace joystick {

struct JoystickDescriptor;

class ActionFactory:
    public QObject,
    public QnCommonModuleAware
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

    QString id() const;

    void updateConfig(const JoystickDescriptor& config);

    void handleStateChanged(const std::vector<double>& stick, const std::vector<bool>& buttons);

signals:
    void actionReady(
        nx::vms::client::desktop::ui::action::IDType id,
        const nx::vms::client::desktop::ui::action::Parameters& parameters);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace joystick
} // namespace nx::vms::client::desktop
