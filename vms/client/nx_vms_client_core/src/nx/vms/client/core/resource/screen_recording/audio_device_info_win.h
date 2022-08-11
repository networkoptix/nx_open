// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPixmap>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API WinAudioDeviceInfo
{
public:
    WinAudioDeviceInfo(const QString& deviceName);
    virtual ~WinAudioDeviceInfo();

    QString fullName() const;
    bool isMicrophone() const;
    QPixmap deviceIcon() const;

    struct Private; //< Declaration needs to be visible from callback.
private:
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
