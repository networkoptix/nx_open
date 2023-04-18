// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace nx::vms::client::desktop::joystick {

struct OsHidDeviceInfo
{
    QString id;
    QString path;
    QString modelName;
    QString manufacturerName;
};

class OsHidDevice: public QObject
{
    Q_OBJECT

public:
    virtual OsHidDeviceInfo info() const = 0;

protected:
    virtual int read(unsigned char* buffer, int bufferSize) = 0;

signals:
    void stateChanged(const QBitArray& newState);
};

} // namespace nx::vms::client::desktop::joystick
