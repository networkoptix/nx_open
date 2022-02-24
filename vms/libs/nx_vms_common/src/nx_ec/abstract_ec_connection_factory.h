// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::time { class AbstractTimeSyncManager; }

namespace ec2 {

class TransactionMessageBusAdapter;

class NX_VMS_COMMON_API AbstractECConnectionFactory: public QObject
{
    Q_OBJECT

public:
    virtual ~AbstractECConnectionFactory() = default;

    virtual void shutdown() = 0;
};

} // namespace ec2
