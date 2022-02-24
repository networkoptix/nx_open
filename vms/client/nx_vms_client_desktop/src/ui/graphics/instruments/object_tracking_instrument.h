// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "click_instrument.h"

/**
 * This instrument listens to click events and sends "start object tracking" command to the server,
 * when it's needed.
 */
class ObjectTrackingInstrument:
    public ClickInstrument,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = ClickInstrument;

public:
    ObjectTrackingInstrument(QObject* parent = nullptr);
    virtual ~ObjectTrackingInstrument() override;
};
