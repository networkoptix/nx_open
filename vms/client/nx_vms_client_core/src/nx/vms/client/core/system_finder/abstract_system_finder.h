// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "system_description.h"

namespace nx::vms::client::core {

/** Interface for classes providing connectable systems information. */
class NX_VMS_CLIENT_CORE_API AbstractSystemFinder: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /** Full list of discivered systems. */
    virtual SystemDescriptionList systems() const = 0;

    /**
     * Find a system by certain id. Logic is defined purely by implementation, see `SystemFinder`
     * class documentation for the examples.
     */
    virtual SystemDescriptionPtr getSystem(const QString& id) const = 0;

signals:
    void systemDiscovered(const SystemDescriptionPtr& system);

    void systemLost(const QString& id, const nx::Uuid& localId);
};

} // namespace nx::vms::client::core
