// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/thread/mutex.h>

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
class Manager: public QObject
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

    JoystickDescriptor createDeviceDescription(const QString& model);

    JoystickDescriptor getDefaultDeviceDescription(const QString& model) const;
    const JoystickDescriptor& getDeviceDescription(const QString& model) const;
    void updateDeviceDescription(const JoystickDescriptor& config);

    /** Save config files. */
    void saveConfig(const QString& model);

    /**
     * If disabled, any device events will not be transmitted to
     * action factory for action creation.
     */
    void setDeviceActionsEnabled(bool enabled);

protected:
    virtual void updateSearchState();

    /** Load config files. */
    void loadConfigs();

    void loadConfig(
        const QDir& searchDir,
        DeviceConfigs& destConfigs,
        QMap<QString, QString>& destIdToFileName) const;

    void loadGeneralConfig(const QDir& searchDir, JoystickDescriptor& destConfig) const;

    virtual void removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths);
    void initializeDevice(const DevicePtr& device, const JoystickDescriptor& description);

    bool isGeneralJoystickConfig(const JoystickDescriptor& config);

    QTimer* pollTimer() const;

    const DeviceConfigs& getKnownJoystickConfigs() const;

private:
    QDir getLocalConfigDirPath() const;

protected:
    mutable nx::Mutex m_mutex;

    QMap<QString, DevicePtr> m_devices;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace joystick
} // namespace nx::vms::client::desktop
