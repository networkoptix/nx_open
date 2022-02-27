// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QSharedPointer>

#include <nx/utils/thread/mutex.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {
namespace joystick {

class ActionFactory;
class Device;
struct JoystickDescriptor;

using ActionFactoryPtr = QSharedPointer<ActionFactory>;
using DevicePtr = QSharedPointer<Device>;
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
   static Manager* create(QObject* parent = 0);

protected:
    Manager(QObject* parent = 0);

public:
    virtual ~Manager();

    /**
     * Get list of recognized HID devices attached to the client PC.
     * This list includes unsupported models of known device types (e.g. joysticks),
     * however devices created for such models are dummy and provide only basic device info
     * such as manufacturer and model identifiers.
     */
    QList<DevicePtr> devices() const;

    JoystickDescriptor getDefaultDeviceDescription(const QString& id) const;
    JoystickDescriptor getDeviceDescription(const QString& id) const;
    void updateDeviceDescription(const JoystickDescriptor& config);

    /** Load config files. */
    void loadConfig();

    /** Save config files. */
    void saveConfig(const QString& id);

    /**
     * If disabled, any device events will not be transmitted to
     * action factory for action creation.
     */
    void setDeviceActionsEnabled(bool enabled);

signals:
    void deviceConnected(const DevicePtr& device);
    void deviceDisconnected(const DevicePtr& device);

protected:
    virtual DevicePtr createDevice(
        const JoystickDescriptor& deviceConfig,
        const QString& path) = 0;
    virtual void enumerateDevices() = 0;
    void loadConfig(
        const QString& searchDir,
        DeviceConfigs& destConfigs,
        QMap<QString, QString>& destIdToRelativePath) const;
    DevicePtr initializeDeviceUnsafe(const JoystickDescriptor& deviceConfig, const QString& path);
    void removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths);
    void tryInitializeDevice(const JoystickDescriptor& description, const QString& devicePath);

protected:
    mutable nx::Mutex m_mutex;

    // Configs for known device models.
    DeviceConfigs m_defaultDeviceConfigs;
    DeviceConfigs m_deviceConfigs;

    QMap<QString, QString> m_deviceConfigRelativePath;

    QMap<QString, DevicePtr> m_devices;
    QMap<QString, ActionFactoryPtr> m_actionFactories;

    QTimer m_enumerateTimer;
    QTimer m_pollTimer;

    bool m_deviceActionsEnabled = true;
};

} // namespace joystick
} // namespace nx::vms::client::desktop
