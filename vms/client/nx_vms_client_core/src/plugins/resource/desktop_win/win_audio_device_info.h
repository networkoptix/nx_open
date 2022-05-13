// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtGui/QPixmap>

class QnWinAudioDeviceInfoPrivate;

class NX_VMS_CLIENT_CORE_API QnWinAudioDeviceInfo
{
public:
    QnWinAudioDeviceInfo(const QString& deviceName);
    virtual ~QnWinAudioDeviceInfo();

    QString fullName() const;
    bool isMicrophone() const;
    QPixmap deviceIcon() const;

private:
    QScopedPointer<QnWinAudioDeviceInfoPrivate> d;
};
