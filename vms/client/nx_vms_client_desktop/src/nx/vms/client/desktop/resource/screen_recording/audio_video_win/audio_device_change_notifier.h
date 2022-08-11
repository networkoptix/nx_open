// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

struct IMMDeviceEnumerator;

namespace nx::vms::client::desktop {

/**
 * Intended for tracking changes in audio devices configuration under Windows OS. Currently only
 * notifies about unplugged device state. Based on Windows Core Audio API.
 * @todo Add more notifications in 4.2 to be able to keep audio configuration consistent and
 *     operable. Including default device change, active/enabled/present device states changes etc.
 */
class AudioDeviceChangeNotifier: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    AudioDeviceChangeNotifier();
    virtual ~AudioDeviceChangeNotifier() override;

signals:

    /**
     * Emitted when peripherals such as microphones, headphones, headsets or speakers disconnect
     *     from an audio device. Do not confuse with device present state change which occurs
     *     when an external audio interface, say USB one, is disconnected.
     * @param deviceName Device name which used to particular device identification.
     * @todo Using device name as ID is unreliable, should be changed to the true device ID here
     *     and everywhere in 4.2.
     */
    void deviceUnplugged(const QString& deviceName);

    /**
     * Emitted when audio peripherals with digital interfaces such as USB headsets or USB web cams
     *    with mic or even USB audio interfaces disconnect from the system.
     * @param deviceName Device name which used to particular device identification.
     * @todo Using device name as ID is unreliable, should be changed to the true device ID here
     *     and everywhere in 4.2.
     */
    void deviceNotPresent(const QString& deviceName);

private:
    IMMDeviceEnumerator* m_deviceEnumerator = nullptr;

    class DeviceChangeListener;
    std::unique_ptr<DeviceChangeListener> m_deviceChangeListener;
};

} // namespace nx::vms::client::desktop
