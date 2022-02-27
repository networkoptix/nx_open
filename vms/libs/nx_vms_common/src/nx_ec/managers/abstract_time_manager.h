// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

namespace ec2 {

class NX_VMS_COMMON_API AbstractTimeNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void primaryTimeServerTimeChanged();
};

} // namespace ec2
