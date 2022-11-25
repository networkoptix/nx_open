// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QSharedPointer>

#include <nx/utils/scoped_connections.h>
#include <nx/utils/thread/mutex.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/utils/impl_ptr.h>

#include "device.h"

namespace nx::vms::client::desktop {
namespace joystick {

class ActionFactory;
struct JoystickDescriptor;

using ActionFactoryPtr = QSharedPointer<ActionFactory>;
using DeviceConfigs = QMap<QString, JoystickDescriptor>;

/**
 * HID Manager is used to monitor human interface devices that are connected to user PC
 * and to organize further interaction with the supported ones.
 */
class Manager:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
   static Manager* create(QObject* parent = nullptr);

protected:
    Manager(QObject* parent = nullptr);

public:
    virtual ~Manager();

    /**
     * Get list of recognized HID devices attached to the client PC.
     * This list includes unsupported models of known device types (e.g. joysticks),
     * however devices created for such models are dummy and provide only basic device info
     * such as manufacturer and model identifiers.
     */
    QList<const Device*> devices() const;

    JoystickDescriptor getDefaultDeviceDescription(const QString& model) const;
    JoystickDescriptor getDeviceDescription(const QString& model) const;
    void updateDeviceDescription(const JoystickDescriptor& config);

    /** Load config files. */
    void loadConfig();

    /** Save config files. */
    void saveConfig(const QString& model);

    /**
     * If disabled, any device events will not be transmitted to
     * action factory for action creation.
     */
    void setDeviceActionsEnabled(bool enabled);

protected:
    virtual void enumerateDevices() = 0;
    void loadConfig(
        const QString& searchDir,
        DeviceConfigs& destConfigs,
        QMap<QString, QString>& destIdToRelativePath) const;

    void loadGeneralConfig(
        const QString& searchDir,
        JoystickDescriptor& destConfig,
        QMap<QString, QString>& destIdToRelativePath) const;

    virtual void removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths);
    void initializeDevice(
        const DevicePtr& device,
        const JoystickDescriptor& description,
        const QString& devicePath);

    bool isGeneralJoystickConfig(const JoystickDescriptor& config);

    QTimer* pollTimer() const;

    const DeviceConfigs& getKnownJoystickConfigs() const;

protected:
    mutable nx::Mutex m_mutex;

    QMap<QString, DevicePtr> m_devices;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace joystick
} // namespace nx::vms::client::desktop
